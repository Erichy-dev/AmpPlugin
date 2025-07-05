cmake_minimum_required(VERSION 3.16)
project(AMP)

# macOS-specific build
set(CMAKE_CXX_STANDARD 17)

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

# Create bundle (MODULE creates .bundle, not .dylib)
add_library(AMP MODULE 
    AMP.cpp 
    Main.cpp
    plugin/user.cpp
    plugin/utilities.cpp
    plugin/search.cpp
    plugin/streamUrl.cpp
    plugin/getFolderList.cpp
    plugin/getFolder.cpp
    plugin/cache.cpp
    plugin/internet.cpp
)

set_target_properties(AMP PROPERTIES
    BUNDLE TRUE
    BUNDLE_EXTENSION "bundle"
    OUTPUT_NAME "AMP"
    PREFIX ""
    SUFFIX ".bundle"
)

# Include directories
target_include_directories(AMP PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

# Link frameworks and libraries
target_link_libraries(AMP PRIVATE
    "-framework CoreFoundation"
    
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Compiler flags
target_compile_options(AMP PRIVATE
    -O2 -Wall
    -fvisibility=hidden
) 