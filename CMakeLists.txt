# RUNE Discord Plugin - CMake Build File
# 
# Integrates DPP (D++ Discord Library) with RUNE Plugin SDK
# Builds DPP with vendored dependencies (openssl, zlib, json) linked statically

cmake_minimum_required(VERSION 3.16)
project(rune_discord_plugin LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Force static MSVC runtime before configuring dependencies
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
endif()

# Enable solution folders in Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(ExternalProject)

# ============================================================================
# Vendored Dependencies
# ============================================================================

# Build zlib from vendored source
set(ZLIB_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendored/zlib)
set(ZLIB_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/zlib-build)
set(ZLIB_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/zlib-install)

ExternalProject_Add(zlib_ext
    SOURCE_DIR ${ZLIB_SOURCE_DIR}
    BINARY_DIR ${ZLIB_BUILD_DIR}
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${ZLIB_INSTALL_DIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DBUILD_SHARED_LIBS=OFF
    BUILD_BYPRODUCTS
        ${ZLIB_INSTALL_DIR}/lib/zs.lib
        ${ZLIB_INSTALL_DIR}/lib/libz.a
)

# Create imported target for zlib
add_library(zlib_static STATIC IMPORTED)
add_dependencies(zlib_static zlib_ext)
if(WIN32)
    set_target_properties(zlib_static PROPERTIES
        IMPORTED_LOCATION ${ZLIB_INSTALL_DIR}/lib/zs.lib
    )
else()
    set_target_properties(zlib_static PROPERTIES
        IMPORTED_LOCATION ${ZLIB_INSTALL_DIR}/lib/libz.a
    )
endif()

# ============================================================================
# OpenSSL - Build from vendored source using ExternalProject
# ============================================================================

set(OPENSSL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendored/openssl)
set(OPENSSL_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/openssl-build)
set(OPENSSL_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/openssl-install)

if(WIN32)
    # Windows: Use nmake with Visual Studio
    ExternalProject_Add(openssl_ext
        SOURCE_DIR ${OPENSSL_SOURCE_DIR}
        BUILD_IN_SOURCE 0
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy_directory ${OPENSSL_SOURCE_DIR} ${OPENSSL_BUILD_DIR}
            COMMAND cd ${OPENSSL_BUILD_DIR} && perl Configure VC-WIN64A no-shared no-asm no-tests --prefix=${OPENSSL_INSTALL_DIR} --openssldir=${OPENSSL_INSTALL_DIR}/ssl
        BUILD_COMMAND cd ${OPENSSL_BUILD_DIR} && nmake
        INSTALL_COMMAND cd ${OPENSSL_BUILD_DIR} && nmake install_sw
        BUILD_BYPRODUCTS
            ${OPENSSL_INSTALL_DIR}/lib/libssl.lib
            ${OPENSSL_INSTALL_DIR}/lib/libcrypto.lib
    )
    set(OPENSSL_SSL_LIBRARY ${OPENSSL_INSTALL_DIR}/lib/libssl.lib)
    set(OPENSSL_CRYPTO_LIBRARY ${OPENSSL_INSTALL_DIR}/lib/libcrypto.lib)
else()
    # Linux/Unix: Use make
    ExternalProject_Add(openssl_ext
        SOURCE_DIR ${OPENSSL_SOURCE_DIR}
        BUILD_IN_SOURCE 0
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy_directory ${OPENSSL_SOURCE_DIR} ${OPENSSL_BUILD_DIR}
            COMMAND cd ${OPENSSL_BUILD_DIR} && ./Configure linux-x86_64 no-shared no-asm no-tests --prefix=${OPENSSL_INSTALL_DIR} --openssldir=${OPENSSL_INSTALL_DIR}/ssl
        BUILD_COMMAND cd ${OPENSSL_BUILD_DIR} && make -j4
        INSTALL_COMMAND cd ${OPENSSL_BUILD_DIR} && make install_sw
        BUILD_BYPRODUCTS
            ${OPENSSL_INSTALL_DIR}/lib/libssl.a
            ${OPENSSL_INSTALL_DIR}/lib/libcrypto.a
            ${OPENSSL_INSTALL_DIR}/lib64/libssl.a
            ${OPENSSL_INSTALL_DIR}/lib64/libcrypto.a
    )
    # OpenSSL may install to lib or lib64 depending on system
    if(EXISTS ${OPENSSL_INSTALL_DIR}/lib64)
        set(OPENSSL_SSL_LIBRARY ${OPENSSL_INSTALL_DIR}/lib64/libssl.a)
        set(OPENSSL_CRYPTO_LIBRARY ${OPENSSL_INSTALL_DIR}/lib64/libcrypto.a)
    else()
        set(OPENSSL_SSL_LIBRARY ${OPENSSL_INSTALL_DIR}/lib/libssl.a)
        set(OPENSSL_CRYPTO_LIBRARY ${OPENSSL_INSTALL_DIR}/lib/libcrypto.a)
    endif()
endif()

# Create imported targets for OpenSSL
add_library(openssl_ssl STATIC IMPORTED)
add_library(openssl_crypto STATIC IMPORTED)
add_dependencies(openssl_ssl openssl_ext)
add_dependencies(openssl_crypto openssl_ext)
set_target_properties(openssl_ssl PROPERTIES IMPORTED_LOCATION ${OPENSSL_SSL_LIBRARY})
set_target_properties(openssl_crypto PROPERTIES IMPORTED_LOCATION ${OPENSSL_CRYPTO_LIBRARY})

# ============================================================================
# Configure DPP with vendored dependencies
# ============================================================================

# DPP must be built as shared on Windows (static lib exceeds 4GB limit)
# but we link OpenSSL/zlib statically INTO the DPP DLL
if(WIN32)
    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
else()
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
endif()

set(DPP_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(DPP_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_VOICE_SUPPORT OFF CACHE BOOL "" FORCE)
set(DPP_NO_VCPKG ON CACHE BOOL "" FORCE)
set(DPP_USE_EXTERNAL_JSON ON CACHE BOOL "" FORCE)
set(DPP_NO_CORO ON CACHE BOOL "" FORCE)

# Tell DPP we're using external dependencies (like Conan would)
# This makes DPP use find_package instead of bundled win32 libs
set(CONAN_EXPORTED ON CACHE BOOL "" FORCE)

# Set up variables for find_package(OpenSSL) and find_package(ZLIB)
# We need to set these before DPP tries to find them, even though the libraries
# haven't been built yet. The paths point to where they will be built.
set(OPENSSL_ROOT_DIR ${OPENSSL_INSTALL_DIR} CACHE PATH "" FORCE)
set(OPENSSL_INCLUDE_DIR ${OPENSSL_INSTALL_DIR}/include CACHE PATH "" FORCE)
# OPENSSL_SSL_LIBRARY and OPENSSL_CRYPTO_LIBRARY are already set above

set(ZLIB_ROOT ${ZLIB_INSTALL_DIR} CACHE PATH "" FORCE)
set(ZLIB_INCLUDE_DIR ${ZLIB_INSTALL_DIR}/include CACHE PATH "" FORCE)
# ZLIB_LIBRARY is set conditionally above

# Create placeholder directories so CMake doesn't complain about non-existent paths
# ExternalProject will create the actual directories during build
file(MAKE_DIRECTORY ${OPENSSL_INSTALL_DIR}/include)
file(MAKE_DIRECTORY ${ZLIB_INSTALL_DIR}/include)

# Create imported targets for OpenSSL and ZLIB to satisfy find_package
# These will be populated with actual library paths after ExternalProject builds them
add_library(OpenSSL::SSL STATIC IMPORTED)
add_library(OpenSSL::Crypto STATIC IMPORTED)
set_target_properties(OpenSSL::SSL PROPERTIES
    IMPORTED_LOCATION ${OPENSSL_SSL_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR}
)
set_target_properties(OpenSSL::Crypto PROPERTIES
    IMPORTED_LOCATION ${OPENSSL_CRYPTO_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR}
)

# DPP expects openssl::openssl target when CONAN_EXPORTED is set
# Create an interface library (not imported) that combines SSL and Crypto
# This allows it to be used in target_link_libraries
add_library(openssl_combined INTERFACE)
target_link_libraries(openssl_combined INTERFACE OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(openssl_combined INTERFACE ${OPENSSL_INCLUDE_DIR})
add_library(openssl::openssl ALIAS openssl_combined)

# DPP also expects Opus::opus target when CONAN_EXPORTED is set
# Since we disabled voice support, create a dummy target
add_library(Opus::opus INTERFACE IMPORTED)
set_target_properties(Opus::opus PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ""
)

add_library(ZLIB::ZLIB STATIC IMPORTED)
if(WIN32)
    set(ZLIB_LIBRARY ${ZLIB_INSTALL_DIR}/lib/zs.lib)
else()
    set(ZLIB_LIBRARY ${ZLIB_INSTALL_DIR}/lib/libz.a)
endif()
set_target_properties(ZLIB::ZLIB PROPERTIES
    IMPORTED_LOCATION ${ZLIB_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${ZLIB_INCLUDE_DIR}
)

# Set all variables that FindOpenSSL.cmake checks for
# Note: We set these even though the libraries don't exist yet (ExternalProject will build them)
# This allows DPP's find_package to succeed during configuration
set(OPENSSL_FOUND TRUE CACHE BOOL "" FORCE)
set(OpenSSL_FOUND TRUE CACHE BOOL "" FORCE)
set(OPENSSL_SSL_FOUND TRUE CACHE BOOL "" FORCE)
set(OPENSSL_CRYPTO_FOUND TRUE CACHE BOOL "" FORCE)
set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} CACHE STRING "" FORCE)
set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR} CACHE PATH "" FORCE)
set(OPENSSL_SSL_LIBRARY ${OPENSSL_SSL_LIBRARY} CACHE FILEPATH "" FORCE)
set(OPENSSL_CRYPTO_LIBRARY ${OPENSSL_CRYPTO_LIBRARY} CACHE FILEPATH "" FORCE)

# Set all variables that FindZLIB.cmake checks for
set(ZLIB_FOUND TRUE CACHE BOOL "" FORCE)
set(ZLIB_LIBRARIES ${ZLIB_LIBRARY} CACHE STRING "" FORCE)
set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR} CACHE PATH "" FORCE)
set(ZLIB_LIBRARY ${ZLIB_LIBRARY} CACHE FILEPATH "" FORCE)

# Create a custom FindOpenSSL module path to override the standard one
# This prevents FindOpenSSL from checking file existence during configuration
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Add DPP subdirectory
add_subdirectory(vendored/DPP)

# Make DPP depend on our external builds
add_dependencies(dpp zlib_ext openssl_ext)

# Manually link our static OpenSSL/zlib to DPP since CONAN_EXPORTED expects
# the targets to exist from find_package
target_link_libraries(dpp PUBLIC ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} ${ZLIB_LIBRARY})
target_include_directories(dpp PUBLIC ${OPENSSL_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR})

# ============================================================================
# Plugin source files
# ============================================================================

set(PLUGIN_SOURCES
    src/discord_plugin.cpp
    src/bot_manager.cpp
    src/nodes/events/on_ready.cpp
    src/nodes/events/on_message.cpp
    src/nodes/events/on_reaction.cpp
    src/nodes/actions/connect_discord.cpp
    src/nodes/actions/send_message.cpp
    src/nodes/actions/send_embed.cpp
    src/nodes/actions/reply_to_message.cpp
    src/nodes/actions/send_direct_message.cpp
    src/nodes/actions/set_presence.cpp
    src/nodes/data/get_user.cpp
    src/nodes/data/get_channel.cpp
    src/nodes/data/build_embed.cpp
)

# Ensure dist directory exists
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dist)

# Create shared library
add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

# Ensure ExternalProjects build before the plugin
add_dependencies(${PROJECT_NAME} zlib_ext openssl_ext)

# Define NODEPLUG_BUILDING to export symbols correctly
target_compile_definitions(${PROJECT_NAME} PRIVATE NODEPLUG_BUILDING)

# Set runtime library to static on Windows
if(MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/rune_plugin_sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendored/DPP/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendored/json/single_include
    ${OPENSSL_INSTALL_DIR}/include
    ${ZLIB_INSTALL_DIR}/include
)

# Link against DPP
target_link_libraries(${PROJECT_NAME} PRIVATE dpp)

# Platform-specific settings
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${PROJECT_NAME}"
    )
    # Link Windows system libraries required by OpenSSL and DPP
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        ws2_32 
        crypt32 
        advapi32 
        user32 
        gdi32
    )
    # Copy DPP DLL to dist (OpenSSL/zlib are now linked statically into DPP)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:dpp> ${CMAKE_CURRENT_SOURCE_DIR}/dist/
        COMMENT "Copying DPP DLL to dist"
    )
elseif(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX "lib"
        SUFFIX ".dylib"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread dl)
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX "lib"
        SUFFIX ".so"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread dl)
endif()

# Set visibility for symbols
if(NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden)
endif()

# Platform-specific compile options
if(MSVC)
    # Add /FS flag for parallel compilation with PDB files
    target_compile_options(${PROJECT_NAME} PRIVATE /FS)
    
    # Suppress common warnings
    target_compile_definitions(${PROJECT_NAME} PRIVATE 
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
    )
endif()

# Output to dist directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dist
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dist
)

# Copy plugin.json to dist directory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/plugin.json)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/plugin.json
            ${CMAKE_CURRENT_SOURCE_DIR}/dist/plugin.json
        COMMENT "Copying plugin.json to dist"
    )
endif()

message(STATUS "Building RUNE Discord Plugin: ${PROJECT_NAME}")
message(STATUS "Output directory: ${CMAKE_CURRENT_SOURCE_DIR}/dist")
message(STATUS "OpenSSL: ${OPENSSL_INSTALL_DIR}")
message(STATUS "Zlib: ${ZLIB_INSTALL_DIR}")
