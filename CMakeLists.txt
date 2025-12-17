# RUNE Discord Plugin - CMake Build File
# 
# Integrates DPP (D++ Discord Library) with RUNE Plugin SDK
# Builds DPP statically using vendored dependencies (openssl, zlib, json)

cmake_minimum_required(VERSION 3.16)
project(rune_discord_plugin LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
        ${ZLIB_INSTALL_DIR}/lib/zlibstatic.lib
        ${ZLIB_INSTALL_DIR}/lib/libz.a
)

# Create imported target for zlib
add_library(zlib_static STATIC IMPORTED)
add_dependencies(zlib_static zlib_ext)
if(WIN32)
    set_target_properties(zlib_static PROPERTIES
        IMPORTED_LOCATION ${ZLIB_INSTALL_DIR}/lib/zlibstatic.lib
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

# On Windows, DPP static lib exceeds 4GB limit, so build as shared DLL
# but link OpenSSL/zlib statically into DPP
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

# Point DPP to our vendored OpenSSL and zlib
set(OPENSSL_INCLUDE_DIR ${OPENSSL_INSTALL_DIR}/include CACHE PATH "" FORCE)
set(OPENSSL_LIBRARIES "${OPENSSL_SSL_LIBRARY};${OPENSSL_CRYPTO_LIBRARY}" CACHE STRING "" FORCE)
set(ZLIB_INCLUDE_DIR ${ZLIB_INSTALL_DIR}/include CACHE PATH "" FORCE)
if(WIN32)
    set(ZLIB_LIBRARIES ${ZLIB_INSTALL_DIR}/lib/zlibstatic.lib CACHE STRING "" FORCE)
else()
    set(ZLIB_LIBRARIES ${ZLIB_INSTALL_DIR}/lib/libz.a CACHE STRING "" FORCE)
endif()

# Add DPP subdirectory
add_subdirectory(vendored/DPP)

# Make DPP depend on our external builds
add_dependencies(dpp zlib_ext openssl_ext)

# ============================================================================
# Plugin source files
# ============================================================================

set(PLUGIN_SOURCES
    src/discord_plugin.cpp
    src/bot_manager.cpp
    src/nodes/events/on_ready.cpp
    src/nodes/events/on_message.cpp
    src/nodes/events/on_reaction.cpp
    src/nodes/actions/send_message.cpp
    src/nodes/actions/send_embed.cpp
    src/nodes/actions/reply_to_message.cpp
    src/nodes/data/get_user.cpp
    src/nodes/data/get_channel.cpp
    src/nodes/data/build_embed.cpp
)

# Create shared library
add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

# Define NODEPLUG_BUILDING to export symbols correctly
target_compile_definitions(${PROJECT_NAME} PRIVATE NODEPLUG_BUILDING)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/rune_plugin_sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendored/DPP/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendored/json/single_include
    ${OPENSSL_INSTALL_DIR}/include
    ${ZLIB_INSTALL_DIR}/include
)

# Link against static DPP and dependencies
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
    # Copy DPP DLL to dist (no external OpenSSL/zlib DLLs needed - they're linked into DPP)
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
