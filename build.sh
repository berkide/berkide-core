#!/usr/bin/env bash
set -e

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"
USE_TLS=OFF

# Parse arguments
# Argumanlari ayristir
for arg in "$@"; do
    case "$arg" in
        --tls) USE_TLS=ON ;;
        Release|Debug|RelWithDebInfo|MinSizeRel) BUILD_TYPE="$arg" ;;
    esac
done

# Configure if needed (incremental build support)
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DBERKIDE_USE_TLS="${USE_TLS}"
else
    # Reconfigure if build type or TLS setting changed
    CACHED_TYPE=$(grep 'CMAKE_BUILD_TYPE:' "${BUILD_DIR}/CMakeCache.txt" | cut -d= -f2)
    CACHED_TLS=$(grep 'BERKIDE_USE_TLS:' "${BUILD_DIR}/CMakeCache.txt" | cut -d= -f2 2>/dev/null || echo "OFF")
    if [ "${CACHED_TYPE}" != "${BUILD_TYPE}" ] || [ "${CACHED_TLS}" != "${USE_TLS}" ]; then
        cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DBERKIDE_USE_TLS="${USE_TLS}"
    fi
fi

# Build (incremental by default)
cmake --build "${BUILD_DIR}" -j "$(nproc 2>/dev/null || sysctl -n hw.ncpu)"

echo "Build complete (${BUILD_TYPE}, TLS=${USE_TLS}). Binary: ${BUILD_DIR}/berkide"

# Copy runtime if exists
if [ -d ".berkide" ] && [ ! -d "${BUILD_DIR}/.berkide" ]; then
    cp -r .berkide "${BUILD_DIR}/.berkide"
    echo "Copied .berkide runtime to build directory."
fi
