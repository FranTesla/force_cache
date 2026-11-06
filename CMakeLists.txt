cmake_minimum_required(VERSION 3.14)
project(version_proxy CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(version SHARED
    src/dllmain.cpp
    version.def
)

set_target_properties(version PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)

install(TARGETS version DESTINATION bin)