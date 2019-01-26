
#undef NDEBUG

#include <iostream>
#include <cassert>
#include <celutil/utf8.h> 

int main()
{
    std::string s("ALF cośtam");
    utf8_string u8s("ALF cośtam");
    assert(u8s == s);
    assert(isSubstring(u8s, s));
    assert(isSubstring(s, u8s));

    std::string s2 = ReplaceGreekLetterAbbr(s);
    utf8_string u8s2 = ReplaceGreekLetterAbbr(u8s.cpp_str());
    assert(u8s2 == s2);
    assert(isSubstring(u8s2, s2));
    assert(isSubstring(s2, u8s2));
    std::cout << "Original std::string: " << s << ", modified std::string: " << s2 << std::endl;
    std::cout << "Original utf8_string: " << u8s << ", modified utf8_string: " << u8s2 << std::endl;
    
    u8s2 += "œ ble ble";
    std::cout << "Extended utf8_string: " << u8s2 << std::endl;
    assert(isSubstring(u8s2, s2));
    assert(!isSubstring(s2, u8s2));
}
