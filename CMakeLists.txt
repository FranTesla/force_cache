cmake_minimum_required(VERSION 3.14)
project(version_proxy CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch MinHook library files (precompiled lib + h)
FetchContent_Declare(
    minhook
    URL https://github.com/TsudaKageyu/minhook/releases/download/v1.3.4/MinHook_134_lib.zip
)
FetchContent_MakeAvailable(minhook)

# Create your proxy DLL target
add_library(version SHARED
    src/dllmain.cpp
    version.def
)

# Link against the static MinHook library (x64)
target_link_libraries(version PRIVATE minhook::minhook-static)

# Ensure MinHook header directory is included in search path
target_include_directories(version PUBLIC
    ${minhook_SOURCE_DIR}/include
)

# Set output filename correctly
set_target_properties(version PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
    DEBUG_POSTFIX "d"
    RELEASE_POSTFIX ""
)

install(TARGETS version DESTINATION bin)