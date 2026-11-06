cmake_minimum_required(VERSION 3.14)
project(version_proxy CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    minhook
    URL https://github.com/TsudaKageyu/minhook/releases/download/v1.3.4/MinHook_134_lib.zip
)

# Fetch and make available to use directly
FetchContent_MakeAvailable(minhook)

add_library(version SHARED
    src/dllmain.cpp
    version.def
)

# Link against the actual MinHook library that was fetched
target_link_libraries(version PRIVATE minhook)

# Set output filename correctly
set_target_properties(version PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
    DEBUG_POSTFIX "d"
    RELEASE_POSTFIX ""
)

install(TARGETS version DESTINATION bin)