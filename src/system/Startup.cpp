#include "Startup.h"
#include "../utils/BerkidePaths.h"
#include "PluginManager.h"
#include "HelpSystem.h"
#include "EditorContext.h"
#include "Logger.h"
#include <thread>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

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


// Start file watcher on user plugins directory for hot-reload
// Canli yeniden yukleme icin kullanici eklenti dizininde dosya izleyiciyi baslat
void StartPluginWatcher(V8Engine& eng) {
    auto& paths = berkide::BerkidePaths::instance();
    eng.startPluginWatcher(paths.userBerkide + "/plugins");
}

// Main editor loop placeholder (runs for 60 seconds as a stub)
// Ana editor dongusu yer tutucusu (taslak olarak 60 saniye calisir)
void StartEditorLoop(V8Engine& eng) {
    LOG_INFO("[Startup] Entering editor loop...");
    using namespace std::chrono_literals;

    // örnek: 60 saniye boyunca çalışıyor
    for (int i = 0; i < 60; ++i) {
        std::this_thread::sleep_for(1s);
        // ileride buraya input handling, command dispatch, vs. gelecek
    }
}

// Gracefully shut down the V8 engine, stop plugin watcher and release resources
// V8 motorunu duzgun kapat, eklenti izleyiciyi durdur ve kaynaklari serbest birak
void ShutdownEngine(V8Engine& eng) {
    eng.stopPluginWatcher();
    eng.shutdown();
    LOG_INFO("[Startup] Engine shutdown complete.");
}