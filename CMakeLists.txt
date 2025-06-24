cmake_minimum_required(VERSION 3.16)
project(AMP)

# macOS-specific build
set(CMAKE_CXX_STANDARD 17)

# Create bundle (MODULE creates .bundle, not .dylib)
add_library(AMP MODULE 
    AMP.cpp 
    Main.cpp
    plugin/user.cpp
    plugin/utilities.cpp
    plugin/search.cpp
    plugin/streamUrl.cpp
)

set_target_properties(AMP PROPERTIES
    BUNDLE TRUE
    BUNDLE_EXTENSION "bundle"
    OUTPUT_NAME "AMP"
    PREFIX ""
    SUFFIX ".bundle"
)

# Link frameworks
target_link_libraries(AMP PRIVATE
    "-framework CoreFoundation"
)

# Compiler flags
target_compile_options(AMP PRIVATE
    -O2 -Wall
    -fvisibility=hidden
) 