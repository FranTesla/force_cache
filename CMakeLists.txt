cmake_minimum_required(VERSION 3.14)
project(version_proxy CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download the official precompiled MinHook v1.3.4 release package directly
include(FetchContent)
FetchContent_Declare(
    minhook
    URL https://github.com/TsudaKageyu/minhook/releases/download/v1.3.4/MinHook_134_lib.zip
)
FetchContent_MakeAvailable(minhook)

# Define an imported static library target pointing to the extracted release files
add_library(minhook STATIC IMPORTED)
set_target_properties(minhook PROPERTIES
    IMPORTED_LOCATION "${minhook_SOURCE_DIR}/lib/libMinHook.x64.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${minhook_SOURCE_DIR}/include"
)

# Create your proxy DLL target
add_library(version SHARED
    src/dllmain.cpp
    version.def
)

# Link against the prebuilt MinHook library
target_link_libraries(version PRIVATE minhook)

# Ensure the output filename is strictly 'version.dll'
set_target_properties(version PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)