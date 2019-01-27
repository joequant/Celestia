//
// C++ Interface: name
//
// Description:
//
//
// Author: Toti <root@totibox>, (C) 2005
//
// Copyright: See COPYING file that comes with this distribution
//
//

#pragma once

#include <string>
#include <iostream>
#include <map>
#include <vector>
#include <celutil/debug.h>
#include <celutil/util.h>
#include <celutil/utf8.h>

typedef std::vector<utf8_string> completion_t;

class NameDatabase
{
 public:
    typedef std::map<utf8_string, uint32_t/*CompareIgnoringCasePredicate*/> NameIndex;
    typedef std::multimap<uint32_t, utf8_string> NumberIndex;
    enum {
        InvalidCatalogNumber = 0xffffffff
    };

 public:
    NameDatabase() {};


    uint32_t getNameCount() const;

    void add(const uint32_t, const utf8_string&);

    // delete all names associated with the specified catalog number
    void erase(const uint32_t);

    uint32_t      getCatalogNumberByName(const utf8_string&) const;
    std::string getNameByCatalogNumber(const uint32_t)       const;

    NumberIndex::const_iterator getFirstNameIter(const uint32_t catalogNumber) const;
    NumberIndex::const_iterator getFinalNameIter() const;

    completion_t getCompletion(const utf8_string& name, bool greek = true) const;
    completion_t getCompletion(const std::vector<std::string>&) const;

    uint32_t findCatalogNumberByName(const utf8_string&) const;
    bool loadNames(std::istream&);

 protected:
    NameIndex   nameIndex;
    NumberIndex numberIndex;

public:
    const NameIndex &getNameIndex() const { return nameIndex; }
    const NumberIndex &getNumberIndex() const { return numberIndex; }
};

