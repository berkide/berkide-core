// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "buffer.h"
#include "cursor.h"
#include "undo.h"
#include "input.h"
#include "EventBus.h"
#include "buffers.h"
#include "file.h"
#include "BerkidePaths.h"
#include "v8_init.h"
#include "V8Engine.h"
#include "Startup.h"
#include "EditorContext.h"
#include "HttpServer.h"
#include "WebSocketServer.h"
#include "ServerConfig.h"
#include "PluginManager.h"
#include "HelpSystem.h"
#include "ProcessManager.h"
#include "RegisterManager.h"
#include "SearchEngine.h"
#include "MarkManager.h"
#include "AutoSave.h"
#include "Extmark.h"
#include "MacroRecorder.h"
#include "KeymapManager.h"
#include "FoldManager.h"
#include "DiffEngine.h"
#include "CompletionEngine.h"
#include "MultiCursor.h"
#include "WindowManager.h"
#include "SessionManager.h"
#include "EncodingDetector.h"
#include "CharClassifier.h"
#include "IndentEngine.h"
#include "WorkerManager.h"
#include "BufferOptions.h"
#include "I18n.h"
#include "Config.h"
#ifdef BERKIDE_TREESITTER_ENABLED
#include "TreeSitterEngine.h"
#endif
#include "Logger.h"
#include <thread>
#include <chrono>
#include <string>
#include <atomic>
#ifdef _WIN32
    #include <process.h>
#else
    #include <unistd.h>
#endif

// Global flag for graceful shutdown
// Duzgun kapatma icin global bayrak
static std::atomic<bool> g_running{true};

// Platform-specific signal/ctrl handling
// Platforma ozgu sinyal/ctrl isleme
#ifdef _WIN32
    #include <windows.h>
    static BOOL WINAPI consoleHandler(DWORD signal) {
        if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
            g_running = false;
            return TRUE;
        }
        return FALSE;
    }
#else
    #include <csignal>
    static void signalHandler(int) {
        g_running = false;
    }
#endif

// Build AppConfig from the unified Config singleton
// Birlesik Config singleton'indan AppConfig olustur
struct AppConfig {
    ServerConfig server;
    bool inspectorEnabled = false;
    bool inspectorBreak = false;
    int inspectorPort = 9229;
    std::string locale = "en";          // UI language / Arayuz dili
};

// Populate AppConfig from Config singleton after all layers are loaded
// Tum katmanlar yuklendikten sonra Config singleton'indan AppConfig'i doldur
static AppConfig buildAppConfig() {
    const Config& cfg = Config::instance();
    AppConfig app;

    app.server.bindAddress = cfg.getString("server.bind_address", "127.0.0.1");
    app.server.httpPort    = cfg.getInt("server.http_port", 1881);
    app.server.wsPort      = cfg.getInt("server.ws_port", 1882);

    std::string token = cfg.getString("server.token", "");
    if (!token.empty()) {
        app.server.bearerToken = token;
        app.server.requireAuth = true;
    }

    app.server.tlsEnabled  = cfg.getBool("server.tls.enabled", false);
    app.server.tlsCertFile = cfg.getString("server.tls.cert", "");
    app.server.tlsKeyFile  = cfg.getString("server.tls.key", "");
    app.server.tlsCaFile   = cfg.getString("server.tls.ca", "NONE");

    app.inspectorEnabled   = cfg.getBool("inspector.enabled", false);
    app.inspectorBreak     = cfg.getBool("inspector.break_on_start", false);
    app.inspectorPort      = cfg.getInt("inspector.port", 9229);
    app.locale             = cfg.getString("locale", "en");

    // Validate: --remote without --token is insecure
    // Dogrulama: --remote --token olmadan guvenli degil
    if (app.server.bindAddress == "0.0.0.0" && app.server.bearerToken.empty()) {
        LOG_WARN("[Startup] --remote without --token is insecure. Use --token=SECRET.");
    }

    // Validate TLS configuration
    // TLS yapilandirmasini dogrula
    if (app.server.tlsEnabled) {
        if (app.server.tlsCertFile.empty() || app.server.tlsKeyFile.empty()) {
            LOG_ERROR("[Startup] TLS requires both --tls-cert= and --tls-key= flags.");
            app.server.tlsEnabled = false;
        }
#ifndef BERKIDE_TLS_ENABLED
        LOG_WARN("[Startup] TLS flags provided but BerkIDE was built without TLS support. Use -DBERKIDE_USE_TLS=ON.");
        app.server.tlsEnabled = false;
#endif
    }

    return app;
}

// BerkIDE main entry point (headless server)
// BerkIDE ana giris noktasi (basliksiz sunucu)
int main(int argc, char *argv[])
{
    // Load config layers: hardcoded defaults -> app config -> user config -> CLI args
    // Config katmanlarini yukle: sabit varsayilanlar -> uygulama config -> kullanici config -> CLI argumanlar
    auto& paths = berkide::BerkidePaths::instance();
    Config& config = Config::instance();
    config.loadFile(paths.appBerkide + "/config.jsonc");     // app defaults / uygulama varsayilanlari
    config.loadFile(paths.userBerkide + "/config.jsonc");    // user override / kullanici gecersiz kilma
    config.applyCliArgs(argc, argv);                         // CLI highest priority / CLI en yuksek oncelik

    AppConfig appCfg = buildAppConfig();

    // Configure logger from config: level + optional file logging
    // Logger'i config'ten yapilandir: seviye + istege bagli dosya loglama
    {
        std::string logLevel = config.getString("log.level", "info");
        if (logLevel == "debug")     Logger::instance().setLevel(LogLevel::Debug);
        else if (logLevel == "warn") Logger::instance().setLevel(LogLevel::Warn);
        else if (logLevel == "error")Logger::instance().setLevel(LogLevel::Error);
        else                         Logger::instance().setLevel(LogLevel::Info);

        if (config.getBool("log.file", false)) {
            std::string logDir = paths.appRoot + "/" + config.getString("log.path", "logs");
            Logger::instance().enableFileLog(logDir);
        }
    }

    // Create all core editor objects
    // Tum temel editor nesnelerini olustur
    Buffers bufs;
    InputHandler input;
    // Use the global singleton so all code (resolveModuleCallback, HttpServer, etc.) sees the same engine
    // Tum kodun (resolveModuleCallback, HttpServer, vb.) ayni motoru gormesi icin global singleton kullan
    V8Engine& eng = V8Engine::instance();
    EventBus event;
    FileSystem file;
    HttpServer httpServer;
    WebSocketServer wsServer;
    PluginManager pluginMgr;
    HelpSystem helpSys;
    ProcessManager procMgr;
    RegisterManager regMgr;
    SearchEngine searchEng;
    MarkManager markMgr;
    AutoSave autoSave;
    ExtmarkManager extmarkMgr;
    MacroRecorder macroRec;
    KeymapManager keymapMgr;
    FoldManager foldMgr;
    DiffEngine diffEng;
    CompletionEngine completionEng;
    MultiCursor multiCur;
    WindowManager winMgr;
    SessionManager sessionMgr;
    EncodingDetector encodingDet;
    CharClassifier charClassifier;
    IndentEngine indentEngine;
    WorkerManager workerMgr;
    BufferOptions bufferOpts;
#ifdef BERKIDE_TREESITTER_ENABLED
    TreeSitterEngine treeSitterEng;
#endif

    // Initialize i18n system and load locale files
    // i18n sistemini baslat ve yerel ayar dosyalarini yukle
    I18n& i18n = I18n::instance();
    i18n.setLocale(appCfg.locale);
    {
        std::string appLocales = paths.appBerkide + "/locales";
        std::string userLocales = paths.userBerkide + "/locales";

        // Load app locales first, then user overrides
        // Once uygulama yerel ayarlarini yukle, sonra kullanici gecersiz kilmalari
        for (const auto& dir : {appLocales, userLocales}) {
            if (std::filesystem::exists(dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    if (entry.path().extension() == ".json") {
                        std::string loc = entry.path().stem().string();
                        i18n.loadLocaleFile(loc, entry.path().string());
                    }
                }
            }
        }
    }

    // EditorContext: connects real objects to V8 bindings
    // EditorContext: gercek nesneleri V8 binding'lerine baglar
    EditorContext edCtx;
    edCtx.buffers       = &bufs;
    edCtx.input         = &input;
    edCtx.eventBus      = &event;
    edCtx.fileSystem    = &file;
    edCtx.httpServer    = &httpServer;
    edCtx.wsServer      = &wsServer;
    edCtx.pluginManager  = &pluginMgr;
    edCtx.helpSystem     = &helpSys;
    edCtx.processManager = &procMgr;
    edCtx.registers      = &regMgr;
    edCtx.searchEngine   = &searchEng;
    edCtx.markManager    = &markMgr;
    edCtx.autoSave       = &autoSave;
    edCtx.extmarkManager = &extmarkMgr;
    edCtx.macroRecorder  = &macroRec;
    edCtx.keymapManager  = &keymapMgr;
    edCtx.foldManager    = &foldMgr;
    edCtx.diffEngine     = &diffEng;
    edCtx.completionEngine = &completionEng;
    edCtx.multiCursor    = &multiCur;
    edCtx.windowManager  = &winMgr;
    edCtx.sessionManager = &sessionMgr;
    edCtx.encodingDetector = &encodingDet;
    edCtx.charClassifier   = &charClassifier;
    edCtx.indentEngine     = &indentEngine;
    edCtx.workerManager    = &workerMgr;
    edCtx.bufferOptions    = &bufferOpts;
    edCtx.i18n             = &i18n;
#ifdef BERKIDE_TREESITTER_ENABLED
    edCtx.treeSitter     = &treeSitterEng;
#endif

    // Wire context to all components that need it
    // Baglami ihtiyac duyan tum bilesenlere bagla
    eng.setEditorContext(edCtx);
    pluginMgr.setEngine(&eng);
    procMgr.setEventBus(&event);
    autoSave.setBuffers(&bufs);
    autoSave.setEventBus(&event);
    autoSave.setDirectory(paths.userBerkide + "/autosave");
    autoSave.setInterval(config.getInt("autosave.interval", 30));
    sessionMgr.setSessionPath(paths.userBerkide + "/session.json");
    httpServer.setEditorContext(&edCtx);
    wsServer.setEditorContext(&edCtx);

    try {
        LOG_INFO("[Startup] BerkIDE starting...");

        // Initialize V8 engine, load plugins, start file watcher
        // V8 motorunu baslat, eklentileri yukle, dosya izleyicisini baslat
        StartEngine(eng);

        // Wire command router for macro playback (available after V8 engine init)
        // Makro oynatma icin komut yonlendiricisini bagla (V8 motoru baslatildiktan sonra mevcut)
        edCtx.commandRouter = &eng.commandRouter();

        // Start V8 Inspector if requested (before loading plugins for breakpoint support)
        // Istenirse V8 Inspector'u baslat (kesme noktasi destegi icin eklentileri yuklemeden once)
        if (appCfg.inspectorEnabled) {
            eng.startInspector(appCfg.inspectorPort, appCfg.inspectorBreak);
        }

        CreateInitBerkideAndLoad(eng);
        LoadBerkideEnvironment(eng);
        StartWatchers();

        // Start HTTP + WS servers
        // HTTP + WS sunucularini baslat
        LOG_INFO("[Startup] Starting servers...");
        httpServer.start(appCfg.server);
        wsServer.start(appCfg.server);

        // Start auto-save background thread
        // Otomatik kaydetme arka plan thread'ini baslat
        autoSave.start();

        LOG_INFO("[Startup] BerkIDE running. Press Ctrl+C to stop.");

        // Register platform-specific signal handlers
        // Platforma ozgu sinyal isleyicilerini kaydet
#ifdef _WIN32
        SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
#endif

        // Main event loop: wait for shutdown signal or restart request
        // Ana olay dongusu: kapatma sinyalini veya yeniden baslatma istegini bekle
        while (g_running) {
            if (g_restartRequested.load()) {
                LOG_INFO("[Startup] File change detected, restarting...");
                break;
            }
            eng.pumpInspectorMessages();
            workerMgr.processPendingMessages();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // Graceful shutdown: save session, kill child processes, then stop servers
        // Duzgun kapatma: oturumu kaydet, once alt surecleri durdur, sonra sunuculari kapat
        LOG_INFO("[Shutdown] Shutting down...");
        workerMgr.terminateAll();
        sessionMgr.save(bufs);
        autoSave.stop();
        procMgr.shutdownAll();
        StopWatchers();
        httpServer.stop();
        wsServer.stop();

        ShutdownEngine(eng);

        // If restart was requested (not Ctrl+C), re-exec the process (nodemon-style)
        // Yeniden baslatma istendiyse (Ctrl+C degil), process'i yeniden calistir (nodemon tarzi)
        if (g_restartRequested.load() && g_running.load()) {
            LOG_INFO("[Startup] Restarting process...");
#ifdef _WIN32
            _execv(argv[0], argv);
#else
            execv(argv[0], argv);
#endif
            // execv only returns on failure
            // execv sadece basarisiz olursa doner
            LOG_ERROR("[Startup] Failed to restart process");
        }

        LOG_INFO("[Shutdown] BerkIDE shut down successfully.");
    }
    catch (const std::exception& e) {
        LOG_ERROR("[Startup] Error: ", e.what());
    }

    return 0;
}
