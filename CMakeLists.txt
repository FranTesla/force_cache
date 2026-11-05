cmake_minimum_required(VERSION 3.14)
project(version_proxy CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Automatically download and integrate MinHook from GitHub during build time
include(FetchContent)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.3
)
FetchContent_MakeAvailable(minhook)

# Fix MinHook's default postfixes so it generates standard 'minhook.lib'
set_target_properties(minhook PROPERTIES
    RELEASE_POSTFIX ""
    DEBUG_POSTFIX ""
    RELWITHDEBINFO_POSTFIX ""
    MINSIZEREL_POSTFIX ""
)

# Create your proxy DLL target using only your local source files
add_library(version SHARED
    src/dllmain.cpp
    version.def
)

# Expose MinHook's public include directory to your target
target_include_directories(version PRIVATE ${minhook_SOURCE_DIR}/include)

# Link against the fetched MinHook library target
target_link_libraries(version PRIVATE minhook)

# Ensure the output filename is strictly 'version.dll'
set_target_properties(version PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)