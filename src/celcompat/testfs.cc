#include <iostream>
#include "fs.h"
namespace fs = celestia::filesystem;

int main()
{
    std::cout << fs::path("/foo/bar.txt").extension() << '\n'
              << fs::path("/foo/bar.").extension() << '\n'
              << fs::path("/foo/bar").extension() << '\n'
              << fs::path("/foo/bar.txt/bar.cc").extension() << '\n'
              << fs::path("/foo/bar.txt/bar.").extension() << '\n'
              << fs::path("/foo/bar.txt/bar").extension() << '\n'
              << fs::path("/foo/.").extension() << '\n'
              << fs::path("/foo/..").extension() << '\n'
              << fs::path("/foo/.hidden").extension() << '\n'
              << fs::path("/foo/..bar").extension() << '\n';

    std::cout << "----------\n";
    std::cout << fs::path("/foo/bar.txt").stem() << '\n'
              << fs::path("/foo/.bar").stem() << '\n';

    for (fs::path p = "foo.bar.baz.tar"; !p.extension().empty(); p = p.stem())
        std::cout << p.extension() << '\n';

    std::cout << "----------\n";
    std::cout << fs::path("/foo/bar.txt") << '\n';
    std::cout << fs::path("baz/foo/bar.txt") << '\n';
    std::cout << fs::path("c:\\foo\\bar.txt") << '\n';
    std::cout << fs::path(L"c:\\foo\\bar.txt") << '\n';
}
