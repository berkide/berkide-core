// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "BerkidePaths.h"
#include "Logger.h"
#include <cstdlib>
#include <filesystem>

#if defined(__APPLE__)
    #include <mach-o/dyld.h>
#elif defined(__linux__)
    #include <unistd.h>
#elif defined(_WIN32)
    #include <windows.h>
#endif

namespace fs = std::filesystem;
namespace berkide {

// Get the directory containing the running executable (cross-platform: macOS, Linux, Windows)
// Calisan yurutulebilir dosyanin bulundugu dizini al (platformlar arasi: macOS, Linux, Windows)
static std::string getExecutableDir() {
    char buf[4096];
    std::string exePath;

#if defined(__APPLE__)
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0)
        exePath = buf;
    else
        exePath = fs::current_path().string();

#elif defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        exePath = buf;
    } else {
        exePath = fs::current_path().string();
    }

#elif defined(_WIN32)
    DWORD len = GetModuleFileNameA(nullptr, buf, sizeof(buf));
    exePath = (len > 0) ? std::string(buf, len) : fs::current_path().string();
#else
    exePath = fs::current_path().string();
#endif

    return fs::path(exePath).parent_path().string();
}

// Return the singleton instance, initializing app and user paths on first call
// Tek ornek nesneyi dondur, ilk cagrimda uygulama ve kullanici yollarini baslat
BerkidePaths& BerkidePaths::instance() {
    static BerkidePaths paths;
    static bool initialized = false;

    if (!initialized) {
        initialized = true;
        paths.appRoot = getExecutableDir();
        paths.appBerkide = (fs::path(paths.appRoot) / ".berkide").string();

        const char* homeEnv = std::getenv("HOME");
#if defined(_WIN32)
        if (!homeEnv) homeEnv = std::getenv("USERPROFILE");
#endif
        paths.userHome = homeEnv ? homeEnv : fs::current_path().string();
        paths.userBerkide = (fs::path(paths.userHome) / ".berkide").string();
    }
    return paths;
}

// Create the user .berkide directory structure (runtime, keymaps, events, plugins, help, autosave, parsers)
// Kullanici .berkide dizin yapisini olustur (runtime, keymaps, events, plugins, help, autosave, parsers)
void BerkidePaths::ensureStructure() {
    using namespace std::filesystem;

    // 1️⃣ User path yapısı oluştur
    for (const auto& sub : {"runtime", "keymaps", "events", "plugins", "help", "autosave", "parsers", "locales"}) {
        create_directories(path(userBerkide) / sub);
    }
}


// Create a directory at the given path if it does not already exist
// Verilen yolda dizin yoksa olustur
void BerkidePaths::ensureDir(const std::string& path) {
    try {
        fs::path p(path);
        if (!fs::exists(p)) {
            fs::create_directories(p);
            LOG_INFO("[berkide] created dir: ", p.string());
        }
    } catch (const std::exception& e) {
        LOG_ERROR("[berkide] failed to create dir: ", path, " (", e.what(), ")");
    }
}

}