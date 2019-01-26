add_executable(utf8 utf8.cpp ${CMAKE_SOURCE_DIR}/src/celutil/utf8.cpp ${CMAKE_SOURCE_DIR}/src/celutil/util.cpp)
target_link_libraries(utf8 tinyutf8)
