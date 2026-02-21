cmake_minimum_required(VERSION 3.16)
project(Berkide VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type defaults to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Platform-conditional compiler flags
# Platforma ozel derleyici bayraklari
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- Core library (shared between main binary and tests) ---
file(GLOB_RECURSE CORE_SOURCES
    src/core/*.cpp
    src/commands/*.cpp
    src/utils/*.cpp
    src/server/*.cpp
    src/system/*.cpp
    src/v8_binding/*.cpp
)

# IXWebSocket sources
file(GLOB IXWS_SOURCES ${CMAKE_SOURCE_DIR}/third_party/ixwebsocket/*.cpp)

add_library(berkide-core STATIC ${CORE_SOURCES} ${IXWS_SOURCES})

target_include_directories(berkide-core PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/third_party/v8/vendor/v8/include
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/v8_binding
    ${CMAKE_SOURCE_DIR}/src/commands
    ${CMAKE_SOURCE_DIR}/src/utils
    ${CMAKE_SOURCE_DIR}/src/system
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/third_party/ixwebsocket
)

# TLS/SSL support (optional, requires OpenSSL)
# TLS/SSL destegi (istege bagli, OpenSSL gerektirir)
option(BERKIDE_USE_TLS "Enable TLS/SSL support for HTTP and WebSocket servers" OFF)

if(BERKIDE_USE_TLS)
    find_package(OpenSSL REQUIRED)
    set(IXWEBSOCKET_USE_TLS ON CACHE BOOL "" FORCE)
    target_compile_definitions(berkide-core PUBLIC
        CPPHTTPLIB_OPENSSL_SUPPORT
        BERKIDE_TLS_ENABLED
    )
    target_link_libraries(berkide-core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    message(STATUS "TLS/SSL support enabled")
else()
    set(IXWEBSOCKET_USE_TLS OFF CACHE BOOL "" FORCE)
endif()

# Tree-sitter support (optional, for syntax parsing)
# Tree-sitter destegi (istege bagli, soz dizimi ayristirma icin)
option(BERKIDE_USE_TREESITTER "Enable tree-sitter syntax parsing" ON)

if(BERKIDE_USE_TREESITTER)
    enable_language(C)
    add_subdirectory(third_party/tree-sitter)
    target_link_libraries(berkide-core PUBLIC tree-sitter)
    target_compile_definitions(berkide-core PUBLIC BERKIDE_TREESITTER_ENABLED)
    message(STATUS "Tree-sitter support enabled")
endif()

# Pass version from CMake to C++ as compile-time define
# CMake'den C++'a derleme zamani tanimlama olarak versiyon aktar
target_compile_definitions(berkide-core PUBLIC
    BERKIDE_VERSION="${PROJECT_VERSION}"
    BERKIDE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    BERKIDE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    BERKIDE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# IXWebSocket settings
set(IXWEBSOCKET_USE_ZLIB OFF CACHE BOOL "" FORCE)

# Platform-conditional libraries
# Platforma ozel kutuphaneler
if(WIN32)
    target_link_libraries(berkide-core PUBLIC
        ${CMAKE_SOURCE_DIR}/third_party/v8/vendor/v8/lib/libv8_monolith.a
        ws2_32
    )
    if(NOT MSVC)
        target_link_libraries(berkide-core PUBLIC pthread dl m)
    endif()
else()
    target_link_libraries(berkide-core PUBLIC
        ${CMAKE_SOURCE_DIR}/third_party/v8/vendor/v8/lib/libv8_monolith.a
        pthread
        dl
        m
    )
    if(APPLE)
        message(STATUS "Building on macOS (universal architecture assumed)")
        target_link_libraries(berkide-core PUBLIC "-framework CoreFoundation")
    elseif(UNIX)
        message(STATUS "Building on Linux")
    endif()
endif()

# --- Main executable ---
add_executable(berkide src/main.cpp)
target_link_libraries(berkide PRIVATE berkide-core)

# Copy .berkide runtime assets to build directory after each build
# Her build sonrasi .berkide calisma zamani dosyalarini build dizinine kopyala
add_custom_command(TARGET berkide POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/.berkide
        $<TARGET_FILE_DIR:berkide>/.berkide
    COMMENT "Copying .berkide runtime to build directory"
)
