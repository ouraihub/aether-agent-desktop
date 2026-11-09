# Project-owned integration. curl-8.23.0/ is an unchanged upstream release.
if(NOT WIN32)
    message(STATUS "aether_curl is unavailable: only Windows Schannel is approved")
    return()
endif()

# Normal directory-scoped variables keep curl's generic options (in particular
# BUILD_TESTING and BUILD_SHARED_LIBS) from changing the parent project's policy.
# CMP0126 also preserves the normal OFF values when upstream declares its
# AUTO/ON/OFF compression dependency cache entries.
set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)
set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)
set(SHARE_LIB_OBJECT OFF)
set(BUILD_CURL_EXE OFF)
set(BUILD_STATIC_CURL OFF)
set(BUILD_TESTING OFF)
set(BUILD_EXAMPLES OFF)
set(CURL_BUILD_EVERYTHING OFF)
set(CURL_DISABLE_INSTALL ON)
set(CURL_ENABLE_EXPORT_TARGET OFF)
set(BUILD_LIBCURL_DOCS OFF)
set(BUILD_MISC_DOCS OFF)
set(ENABLE_CURL_MANUAL OFF)
set(CMAKE_DISABLE_FIND_PACKAGE_Perl TRUE)

set(PICKY_COMPILER OFF)
set(CURL_WERROR OFF)
set(CURL_CLANG_TIDY OFF)
set(CURL_GCC_ANALYZER OFF)
set(CURL_CODE_COVERAGE OFF)
set(CURL_LINT OFF)
set(ENABLE_DEBUG OFF)
set(CURL_DEBUG_GLOBAL_MEM OFF)
set(CURL_USE_LIBBACKTRACE OFF)
set(CURL_USE_LIBUV OFF)
set(CURL_STATIC_CRT OFF)

# Windows owns TLS, certificate verification and resolver threads. There is no
# find_package(CURL), alternate TLS backend, or system libcurl fallback.
set(CURL_ENABLE_SSL ON)
set(CURL_USE_SCHANNEL ON)
set(CURL_DEFAULT_SSL_BACKEND "schannel")
set(CURL_WINDOWS_SSPI ON)
set(CURL_CA_NATIVE ON)
set(CURL_DISABLE_CA_SEARCH ON)
set(CURL_USE_OPENSSL OFF)
set(CURL_USE_MBEDTLS OFF)
set(CURL_USE_WOLFSSL OFF)
set(CURL_USE_GNUTLS OFF)
set(CURL_USE_RUSTLS OFF)
set(ENABLE_ARES OFF)
set(ENABLE_THREADED_RESOLVER ON)
set(ENABLE_UNIX_SOCKETS OFF)
set(USE_HTTPSRR OFF)
set(USE_ECH OFF)
set(USE_SSLS_EXPORT OFF)
set(USE_PROXY_HTTP3 OFF)

# Do not probe or silently pick up optional libraries from the host machine.
set(CURL_USE_CMAKECONFIG OFF)
set(CURL_USE_PKGCONFIG OFF)
set(CURL_ZLIB OFF)
set(CURL_BROTLI OFF)
set(CURL_ZSTD OFF)
set(USE_NGHTTP2 OFF)
set(USE_NGTCP2 OFF)
set(USE_QUICHE OFF)
set(USE_LIBIDN2 OFF)
set(USE_WIN32_IDN OFF)
set(USE_WIN32_LDAP OFF)
set(CURL_USE_LIBPSL OFF)
set(CURL_USE_LIBSSH2 OFF)
set(CURL_USE_LIBSSH OFF)
set(CURL_USE_GSASL OFF)
set(CURL_USE_GSSAPI OFF)

# Keep only HTTP and HTTPS, including no HTTP-based alternate protocols.
set(CURL_DISABLE_HTTP OFF)
foreach(protocol IN ITEMS DICT FILE FTP GOPHER IMAP LDAP LDAPS MQTT POP3 RTSP
        SMTP TELNET TFTP IPFS WEBSOCKETS DOH)
    set(CURL_DISABLE_${protocol} ON)
endforeach()

add_subdirectory(curl-8.23.0 EXCLUDE_FROM_ALL)
add_library(aether_curl ALIAS libcurl_static)

# Upstream exports curl/curl.h, CURL_STATICLIB and its native system link
# dependencies. Suppress warnings only for its translation units, as for the
# existing SQLite/yyjson targets; never apply first-party /WX to curl.
set_target_properties(libcurl_static PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
target_include_directories(libcurl_static SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/curl-8.23.0/include")
if(MSVC)
    target_compile_options(libcurl_static PRIVATE /W0)
else()
    target_compile_options(libcurl_static PRIVATE -w)
endif()
