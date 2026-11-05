cmake_minimum_required(VERSION 3.14)
project(version_proxy CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch MinHook source code automatically
include(FetchContent)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.3
)
FetchContent_MakeAvailable(minhook)

# Build version.dll and compile MinHook sources directly together
add_library(version SHARED
    src/dllmain.cpp
    version.def
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/buffer.h
    ${minhook_SOURCE_DIR}/src/trampoline.c
    ${minhook_SOURCE_DIR}/src/trampoline.h
    ${minhook_SOURCE_DIR}/src/hde/hde32.c
    ${minhook_SOURCE_DIR}/src/hde/hde32.h
    ${minhook_SOURCE_DIR}/src/hde/hde64.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.h
    ${minhook_SOURCE_DIR}/src/hde/pstdint.h
    ${minhook_SOURCE_DIR}/src/hde/table.h
    ${minhook_SOURCE_DIR}/src/hook.c
)

# Include MinHook headers and internal paths
target_include_directories(version PRIVATE 
    ${minhook_SOURCE_DIR}/include
    ${minhook_SOURCE_DIR}/src
    ${minhook_SOURCE_DIR}/src/hde
)

# Ensure the output filename is strictly 'version.dll'
set_target_properties(version PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)
