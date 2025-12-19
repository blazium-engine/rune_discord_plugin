# Custom FindOpenSSL module for rune_discord_plugin
# This module uses pre-configured OpenSSL paths from ExternalProject
# It doesn't check for file existence during configuration since ExternalProject
# builds the libraries at build time, not configuration time.

# If variables are already set (by our CMakeLists.txt), use them
if(OPENSSL_SSL_LIBRARY AND OPENSSL_CRYPTO_LIBRARY AND OPENSSL_INCLUDE_DIR)
    set(OPENSSL_FOUND TRUE)
    set(OpenSSL_FOUND TRUE)
    set(OPENSSL_SSL_FOUND TRUE)
    set(OPENSSL_CRYPTO_FOUND TRUE)
    set(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
    set(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR})
    
    # Create imported targets if they don't exist
    if(NOT TARGET OpenSSL::SSL)
        add_library(OpenSSL::SSL STATIC IMPORTED)
        set_target_properties(OpenSSL::SSL PROPERTIES
            IMPORTED_LOCATION ${OPENSSL_SSL_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR}
        )
    endif()
    
    if(NOT TARGET OpenSSL::Crypto)
        add_library(OpenSSL::Crypto STATIC IMPORTED)
        set_target_properties(OpenSSL::Crypto PROPERTIES
            IMPORTED_LOCATION ${OPENSSL_CRYPTO_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR}
        )
    endif()
    
    # Set version (required by some find_package checks)
    if(NOT OPENSSL_VERSION)
        set(OPENSSL_VERSION "3.0.0")
    endif()
else()
    # Fall back to standard FindOpenSSL if variables aren't set
    include(${CMAKE_ROOT}/Modules/FindOpenSSL.cmake)
endif()

