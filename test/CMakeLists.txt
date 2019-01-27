add_executable(utf8 utf8.cpp)
target_link_libraries(utf8 celutil tinyutf8)

add_executable(name name.cpp)
target_link_libraries(name celengine celutil tinyutf8)

add_executable(greek greek.cpp)
target_link_libraries(greek celutil tinyutf8)
