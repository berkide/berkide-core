// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "Startup.h"
#include "../utils/BerkidePaths.h"
#include "PluginManager.h"
#include "HelpSystem.h"
#include "EditorContext.h"
#include "FileWatcher.h"
#include "Logger.h"
#include <thread>
#include <chrono>
#include <filesystem>
#include <vector>
#include <memory>

namespace fs = std::filesystem;

// Global restart flag (extern declared in Startup.h)
// Global yeniden baslatma bayragi (Startup.h'de extern tanimli)
std::atomic<bool> g_restartRequested{false};

// File watchers owned by Startup (not V8Engine)
// Startup'a ait dosya izleyicileri (V8Engine degil)
static std::vector<std::unique_ptr<FileWatcher>> s_watchers;

// Initialize the V8 JavaScript engine, throw on failure
// V8 JavaScript motorunu baslat, basarisiz olursa hata firlat
void StartEngine(V8Engine& eng) {
    if (eng.initialize())
        LOG_INFO("[Startup] Engine initialized.");
    else
        throw std::runtime_error("Engine initialization failed");
}

// Create the .berkide directory structure and prepare for plugin loading
// .berkide dizin yapisini olustur ve eklenti yuklemesi icin hazirla
void CreateInitBerkideAndLoad(V8Engine& eng) {
    auto& paths = berkide::BerkidePaths::instance();
    paths.ensureStructure();

    LOG_INFO("[Startup] App runtime: ", paths.appBerkide);
    LOG_INFO("[Startup] User runtime: ", paths.userBerkide);
}

// Load scripts from user or app .berkide directories in order: runtime, keymaps, events, then PluginManager
// Kullanici veya uygulama .berkide dizinlerinden sirayla betikleri yukle: runtime, keymaps, events, sonra PluginManager
void LoadBerkideEnvironment(V8Engine& eng) {
    auto& paths = berkide::BerkidePaths::instance();

    auto prefer = [&](const std::string& subdir, bool recursive) {
        std::string userDir = paths.userBerkide + "/" + subdir;
        std::string appDir  = paths.appBerkide  + "/" + subdir;

        if (std::filesystem::exists(userDir) && !std::filesystem::is_empty(userDir)) {
            eng.loadAllScripts(userDir, recursive);
        } else if (std::filesystem::exists(appDir) && !std::filesystem::is_empty(appDir)) {
            eng.loadAllScripts(appDir, recursive);
        } else {
            LOG_INFO("[Berkide] (", subdir, ") kaynak yok.");
        }
    };

    // Sira: runtime -> keymaps -> events (plugins are loaded via PluginManager)
    // Sira: runtime -> keymaps -> events (eklentiler PluginManager uzerinden yuklenir)
    prefer("runtime",  false);
    prefer("keymaps",  true);
    prefer("events",   true);

    // Use PluginManager for plugin loading (with dependency resolution)
    // Eklenti yuklemesi icin PluginManager kullan (bagimlilik cozumu ile)
    auto* ctx = eng.editorContext();
    if (ctx && ctx->pluginManager) {
        std::string userPlugins = paths.userBerkide + "/plugins";
        std::string appPlugins  = paths.appBerkide  + "/plugins";

        if (fs::exists(userPlugins)) ctx->pluginManager->discover(userPlugins);
        if (fs::exists(appPlugins))  ctx->pluginManager->discover(appPlugins);

        ctx->pluginManager->loadAll();
    }

    // Load help system from help directory
    // Yardim sistemini yardim dizininden yukle
    if (ctx && ctx->helpSystem) {
        std::string userHelp = paths.userBerkide + "/help";
        std::string appHelp  = paths.appBerkide  + "/help";
        if (fs::exists(userHelp)) ctx->helpSystem->loadFromDirectory(userHelp);
        if (fs::exists(appHelp))  ctx->helpSystem->loadFromDirectory(appHelp);
    }
}

// Start file watchers on both app and user .berkide directories
// Hem app hem user .berkide dizinlerinde dosya izleyicileri baslat
// On any file change, sets g_restartRequested so main loop can restart the process
// Herhangi bir dosya degisikliginde g_restartRequested set eder, boylece ana dongu process'i yeniden baslatabilir
void StartWatchers() {
    auto& paths = berkide::BerkidePaths::instance();

    auto addWatcher = [](const std::string& dir) {
        if (!fs::exists(dir)) {
            LOG_WARN("[Watcher] Directory does not exist, skipping: ", dir);
            return;
        }

        auto watcher = std::make_unique<FileWatcher>();

        watcher->onEvent([](const FileEventData& event) {
            const char* action = event.type == FileEvent::Created  ? "Created"  :
                                 event.type == FileEvent::Modified ? "Modified" :
                                                                     "Deleted";
            LOG_INFO("[Watcher] ", action, ": ", event.path);
            g_restartRequested.store(true);
        });

        watcher->watch(dir);
        s_watchers.push_back(std::move(watcher));
    };

    addWatcher(paths.appBerkide);
    addWatcher(paths.userBerkide);
}

// Stop all file watchers
// Tum dosya izleyicilerini durdur
void StopWatchers() {
    for (auto& w : s_watchers) {
        w->stop();
    }
    s_watchers.clear();
    LOG_INFO("[Watcher] All watchers stopped");
}

// Main editor loop placeholder (runs for 60 seconds as a stub)
// Ana editor dongusu yer tutucusu (taslak olarak 60 saniye calisir)
void StartEditorLoop(V8Engine& eng) {
    LOG_INFO("[Startup] Entering editor loop...");
    using namespace std::chrono_literals;

    for (int i = 0; i < 60; ++i) {
        std::this_thread::sleep_for(1s);
    }
}

// Gracefully shut down the V8 engine
// V8 motorunu duzgun kapat
void ShutdownEngine(V8Engine& eng) {
    eng.shutdown();
    LOG_INFO("[Startup] Engine shutdown complete.");
}
