cmake_minimum_required(VERSION 3.14)
project(version_proxy CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/MinHook.git
    GIT_TAG v1.3.4
)

FetchContent_MakeAvailable(minhook)

add_library(version SHARED
    src/dllmain.cpp
    version.def
)

target_link_libraries(version PRIVATE minhook::minhook-static)

set_target_properties(version PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)

install(TARGETS version DESTINATION bin)