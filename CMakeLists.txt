cmake_minimum_required(VERSION 3.8)
project(serial)


find_package(Boost COMPONENTS system)

add_executable(serial main.cpp)
target_link_libraries(serial Boost::system)
set_property(TARGET serial PROPERTY CXX_STANDARD 17)
