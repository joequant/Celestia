//
// C++ Implementation: dsodb
//
// Description:
//
//
// Author: Toti <root@totibox>, (C) 2005
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include <cmath>
#include <cstdlib>
#include <cassert>
#include <algorithm>
#include <celutil/debug.h>
#include <celmath/mathlib.h>
#include <celutil/gettext.h>
#include <celutil/bytes.h>
#include <celutil/utf8.h>
#include <celengine/dsodb.h>
#include <config.h>
#include "astro.h"
#include "parser.h"
#include "parseobject.h"
#include "multitexture.h"
#include "meshmanager.h"
#include "tokenizer.h"
#include <celutil/debug.h>

#include <celengine/galaxy.h>
#include <celengine/globular.h>
#include <celengine/opencluster.h>
#include <celengine/nebula.h>

#include <Eigen/Core>
#include <Eigen/Geometry>

using namespace Eigen;
using namespace std;


constexpr const float DSO_OCTREE_MAGNITUDE   = 8.0f;
//constexpr const float DSO_EXTRA_ROOM         = 0.01f; // Reserve 1% capacity for extra DSOs
                                                      // (useful as a complement of binary loaded DSOs)

constexpr char FILE_HEADER[]                 = "CEL_DSOs";

// Used to sort DSO pointers by catalog number
struct PtrCatalogNumberOrderingPredicate
{
    int unused;

    PtrCatalogNumberOrderingPredicate() = default;

    bool operator()(const DeepSkyObject* const & dso0, const DeepSkyObject* const & dso1) const
    {
        return (dso0->getIndex() < dso1->getIndex());
    }
};


DSODatabase::~DSODatabase()
{
    delete [] DSOs;
    delete [] catalogNumberIndex;
}


DeepSkyObject* DSODatabase::find(const AstroCatalog::IndexNumber catalogNumber) const
{
    Galaxy refDSO;  //terrible hack !!
    refDSO.setIndex(catalogNumber);

    DeepSkyObject** dso   = lower_bound(catalogNumberIndex,
                                        catalogNumberIndex + nDSOs,
                                        &refDSO,
                                        PtrCatalogNumberOrderingPredicate());

    if (dso != catalogNumberIndex + nDSOs && (*dso)->getIndex() == catalogNumber)
        return *dso;
    else
        return nullptr;
}


DeepSkyObject* DSODatabase::find(const string& name) const
{
    if (name.empty())
        return nullptr;

    if (namesDB != nullptr)
    {
        AstroCatalog::IndexNumber catalogNumber   = namesDB->findCatalogNumberByName(name);
        if (catalogNumber != AstroCatalog::InvalidIndex)
            return find(catalogNumber);
    }

    return nullptr;
}


vector<string> DSODatabase::getCompletion(const string& name) const
{
    vector<string> completion;

    // only named DSOs are supported by completion.
    if (!name.empty() && namesDB != nullptr)
        return namesDB->getCompletion(name);
    else
        return completion;
}


string DSODatabase::getDSOName(const DeepSkyObject* const & dso, bool i18n) const
{
    AstroCatalog::IndexNumber catalogNumber    = dso->getIndex();

    if (namesDB != nullptr)
    {
        DSONameDatabase::NumberIndex::const_iterator iter   = namesDB->getFirstNameIter(catalogNumber);
        if (iter != namesDB->getFinalNameIter() && iter->first == catalogNumber)
        {
            if (i18n && iter->second != _(iter->second.c_str()))
                return _(iter->second.c_str());
            else
                return iter->second;
        }
    }

    return "";
}


string DSODatabase::getDSONameList(const DeepSkyObject* const & dso, const unsigned int maxNames) const
{
    string dsoNames;

    auto catalogNumber   = dso->getIndex();

    DSONameDatabase::NumberIndex::const_iterator iter  = namesDB->getFirstNameIter(catalogNumber);

    unsigned int count = 0;
    while (iter != namesDB->getFinalNameIter() && iter->first == catalogNumber && count < maxNames)
    {
        if (count != 0)
            dsoNames   += " / ";

        dsoNames   += iter->second;
        ++iter;
        ++count;
    }

    return dsoNames;
}


void DSODatabase::findVisibleDSOs(DSOHandler&    dsoHandler,
                                  const Vector3d& obsPos,
                                  const Quaternionf& obsOrient,
                                  float fovY,
                                  float aspectRatio,
                                  float limitingMag,
                                  OctreeProcStats *stats) const
{
    // Compute the bounding planes of an infinite view frustum
    Hyperplane<double, 3> frustumPlanes[5];
    Vector3d  planeNormals[5];

    Quaterniond obsOrientd = obsOrient.cast<double>();
    Matrix3d    rot    = obsOrientd.toRotationMatrix().transpose();
    double      h      = tan(fovY / 2);
    double      w      = h * aspectRatio;

    planeNormals[0] = Vector3d( 0,  1, -h);
    planeNormals[1] = Vector3d( 0, -1, -h);
    planeNormals[2] = Vector3d( 1,  0, -w);
    planeNormals[3] = Vector3d(-1,  0, -w);
    planeNormals[4] = Vector3d( 0,  0, -1);

    for (int i = 0; i < 5; ++i)
    {
        planeNormals[i]    = rot * planeNormals[i].normalized();
        frustumPlanes[i]   = Hyperplane<double, 3>(planeNormals[i], obsPos);
    }

    octreeRoot->processVisibleObjects(dsoHandler,
                                      obsPos,
                                      frustumPlanes,
                                      limitingMag,
                                      DSO_OCTREE_ROOT_SIZE,
                                      stats);
}


void DSODatabase::findCloseDSOs(DSOHandler&     dsoHandler,
                                const Vector3d& obsPos,
                                float           radius) const
{
    octreeRoot->processCloseObjects(dsoHandler,
                                    obsPos,
                                    radius,
                                    DSO_OCTREE_ROOT_SIZE);
}


DSONameDatabase* DSODatabase::getNameDatabase() const
{
    return namesDB;
}


void DSODatabase::setNameDatabase(DSONameDatabase* _namesDB)
{
    namesDB    = _namesDB;
}


bool DSODatabase::load(istream& in, const fs::path& resourcePath)
{
    Tokenizer tokenizer(&in);
    Parser    parser(&tokenizer);

#ifdef ENABLE_NLS
    const char *d = resourcePath.string().c_str();
    bindtextdomain(d, d); // domain name is the same as resource path
#endif

    while (tokenizer.nextToken() != Tokenizer::TokenEnd)
    {
        string objType;
        string objName;

        if (tokenizer.getTokenType() != Tokenizer::TokenName)
        {
            DPRINTF(LOG_LEVEL_ERROR, "Error parsing deep sky catalog file.\n");
            return false;
        }
        objType = tokenizer.getNameValue();

        bool autoGenCatalogNumber = true;
        AstroCatalog::IndexNumber objCatalogNumber = AstroCatalog::InvalidIndex;
        if (tokenizer.getTokenType() == Tokenizer::TokenNumber)
        {
            autoGenCatalogNumber   = false;
            objCatalogNumber       = (AstroCatalog::IndexNumber) tokenizer.getNumberValue();
            tokenizer.nextToken();
        }

        if (autoGenCatalogNumber)
        {
            objCatalogNumber   = nextAutoCatalogNumber--;
        }

        if (tokenizer.nextToken() != Tokenizer::TokenString)
        {
            DPRINTF(LOG_LEVEL_ERROR, "Error parsing deep sky catalog file: bad name.\n");
            return false;
        }
        objName = tokenizer.getStringValue();

        Value* objParamsValue    = parser.readValue();
        if (objParamsValue == nullptr ||
            objParamsValue->getType() != Value::HashType)
        {
            DPRINTF(LOG_LEVEL_ERROR, "Error parsing deep sky catalog entry %s\n", objName.c_str());
            return false;
        }

        Hash* objParams    = objParamsValue->getHash();
        assert(objParams != nullptr);

        DeepSkyObject* obj = nullptr;
        if (compareIgnoringCase(objType, "Galaxy") == 0)
            obj = new Galaxy();
        else if (compareIgnoringCase(objType, "Globular") == 0)
            obj = new Globular();
        else if (compareIgnoringCase(objType, "Nebula") == 0)
            obj = new Nebula();
        else if (compareIgnoringCase(objType, "OpenCluster") == 0)
            obj = new OpenCluster();

        if (obj != nullptr && obj->load(objParams, resourcePath))
        {
            obj->loadCategories(objParams, DataDisposition::Add, resourcePath.string());
            delete objParamsValue;

            // Ensure that the DSO array is large enough
            if (nDSOs == capacity)
            {
                // Grow the array by 5%--this may be too little, but the
                // assumption here is that there will be small numbers of
                // DSOs in text files added to a big collection loaded from
                // a binary file.
                capacity = (int) (capacity * 1.05);

                // 100 DSOs seems like a reasonable minimum
                if (capacity < 100)
                    capacity = 100;

                DeepSkyObject** newDSOs = new DeepSkyObject*[capacity];

                if (DSOs != nullptr)
                {
                    copy(DSOs, DSOs + nDSOs, newDSOs);
                    delete[] DSOs;
                }
                DSOs = newDSOs;
            }

            DSOs[nDSOs++] = obj;

            obj->setIndex(objCatalogNumber);

            if (namesDB != nullptr && !objName.empty())
            {
                // List of names will replace any that already exist for
                // this DSO.
                namesDB->erase(objCatalogNumber);

                // Iterate through the string for names delimited
                // by ':', and insert them into the DSO database.
                // Note that db->add() will skip empty names.
                string::size_type startPos   = 0;
                while (startPos != string::npos)
                {
                    string::size_type next    = objName.find(':', startPos);
                    string::size_type length  = string::npos;
                    if (next != string::npos)
                    {
                        length = next - startPos;
                        ++next;
                    }
                    string DSOName = objName.substr(startPos, length);
                    namesDB->add(objCatalogNumber, DSOName);
                    if (DSOName != _(DSOName.c_str()))
                        namesDB->add(objCatalogNumber, _(DSOName.c_str()));
                    startPos   = next;
                }
            }
        }
        else
        {
            DPRINTF(LOG_LEVEL_WARNING, "Bad Deep Sky Object definition--will continue parsing file.\n");
            delete objParamsValue;
            return false;
        }
    }
    return true;
}


bool DSODatabase::loadBinary(istream&)
{
    // TODO: define a binary dso file format
    return true;
}


void DSODatabase::finish()
{
    buildOctree();
    buildIndexes();
    calcAvgAbsMag();
    /*
    // Put AbsMag = avgAbsMag for Add-ons without AbsMag entry
    for (int i = 0; i < nDSOs; ++i)
    {
        if(DSOs[i]->getAbsoluteMagnitude() == DSO_DEFAULT_ABS_MAGNITUDE)
            DSOs[i]->setAbsoluteMagnitude((float)avgAbsMag);
    }
    */
    fmt::fprintf(clog, _("Loaded %i deep space objects\n"), nDSOs);
}


void DSODatabase::buildOctree()
{
    DPRINTF(LOG_LEVEL_INFO, "Sorting DSOs into octree . . .\n");
    float absMag             = astro::appToAbsMag(DSO_OCTREE_MAGNITUDE, DSO_OCTREE_ROOT_SIZE * (float) sqrt(3.0));

    // TODO: investigate using a different center--it's possible that more
    // objects end up straddling the base level nodes when the center of the
    // octree is at the origin.
    DynamicDSOOctree* root   = new DynamicDSOOctree(Vector3d::Zero(), absMag);
    for (int i = 0; i < nDSOs; ++i)
    {
        root->insertObject(DSOs[i], DSO_OCTREE_ROOT_SIZE);
    }

    DPRINTF(LOG_LEVEL_INFO, "Spatially sorting DSOs for improved locality of reference . . .\n");
    DeepSkyObject** sortedDSOs    = new DeepSkyObject*[nDSOs];
    DeepSkyObject** firstDSO      = sortedDSOs;

    // The spatial sorting part is useless for DSOs since we
    // are storing pointers to objects and not the objects themselves:
    root->rebuildAndSort(octreeRoot, firstDSO);

    DPRINTF(LOG_LEVEL_INFO, "%d DSOs total\n", (int) (firstDSO - sortedDSOs));
    DPRINTF(LOG_LEVEL_INFO, "Octree has %d nodes and %d DSOs.\n",
            1 + octreeRoot->countChildren(), octreeRoot->countObjects());
    //cout<<"DSOs:  "<< octreeRoot->countObjects()<<"   Nodes:"
    //    <<octreeRoot->countChildren() <<endl;
    // Clean up . . .
    delete[] DSOs;
    delete   root;

    DSOs = sortedDSOs;
}

void DSODatabase::calcAvgAbsMag()
{
    uint32_t nDSOeff = size();
    for (int i = 0; i < nDSOs; ++i)
    {
        double DSOmag = DSOs[i]->getAbsoluteMagnitude();

        // take only DSO's with realistic AbsMag entry
        // (> DSO_DEFAULT_ABS_MAGNITUDE) into account
        if (DSOmag > DSO_DEFAULT_ABS_MAGNITUDE)
            avgAbsMag += DSOmag;
        else if (nDSOeff > 1)
            nDSOeff--;
        //cout << nDSOs<<"  "<<DSOmag<<"  "<<nDSOeff<<endl;
    }
    avgAbsMag /= (double) nDSOeff;
    //cout<<avgAbsMag<<endl;
}


void DSODatabase::buildIndexes()
{
    // This should only be called once for the database
    // assert(catalogNumberIndexes[0] == nullptr);

    DPRINTF(LOG_LEVEL_INFO, "Building catalog number indexes . . .\n");

    catalogNumberIndex = new DeepSkyObject*[nDSOs];
    for (int i = 0; i < nDSOs; ++i)
        catalogNumberIndex[i] = DSOs[i];

    sort(catalogNumberIndex, catalogNumberIndex + nDSOs, PtrCatalogNumberOrderingPredicate());
}


double DSODatabase::getAverageAbsoluteMagnitude() const
{
    return avgAbsMag;
}
