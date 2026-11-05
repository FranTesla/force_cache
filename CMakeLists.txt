cmake_minimum_required(VERSION 3.14)
project(version_proxy CXX)

set(CMAKE_CXX_STANDARD 17)

# Automatically download and integrate MinHook from GitHub during build time
include(FetchContent)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.3
)
FetchContent_MakeAvailable(minhook)

# Create your proxy DLL target
add_library(version SHARED
    src/dllmain.cpp
    version.def
)

# Link MinHook automatically (MinHook's CMake target is named 'minhook')
target_link_libraries(version PRIVATE minhook)

# Ensure the output filename is strictly 'version.dll'
set_target_properties(version PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)