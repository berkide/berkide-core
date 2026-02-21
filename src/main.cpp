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
#ifdef BERKIDE_TREESITTER_ENABLED
#include "TreeSitterEngine.h"
#endif
#include "Logger.h"
#include <thread>
#include <chrono>
#include <string>
#include <atomic>

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

// Application configuration parsed from CLI arguments
// CLI argumanlarindan ayristirilan uygulama yapilandirmasi
struct AppConfig {
    ServerConfig server;
    bool inspectorEnabled = false;
    bool inspectorBreak = false;
    int inspectorPort = 9229;
};

// Parse command line arguments into AppConfig
// Komut satiri argumanlarini AppConfig'e ayristir
// Supported flags: --remote, --token=, --http-port=, --ws-port=, --port=, --tls-cert=, --tls-key=, --tls-ca=
// Desteklenen bayraklar: --remote, --token=, --http-port=, --ws-port=, --port=, --tls-cert=, --tls-key=, --tls-ca=
static AppConfig parseArgs(int argc, char* argv[]) {
    AppConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--remote") {
            cfg.server.bindAddress = "0.0.0.0";
        } else if (arg.rfind("--token=", 0) == 0) {
            cfg.server.bearerToken = arg.substr(8);
            cfg.server.requireAuth = true;
        } else if (arg.rfind("--http-port=", 0) == 0) {
            cfg.server.httpPort = std::stoi(arg.substr(12));
        } else if (arg.rfind("--ws-port=", 0) == 0) {
            cfg.server.wsPort = std::stoi(arg.substr(10));
        } else if (arg.rfind("--port=", 0) == 0) {
            cfg.server.httpPort = std::stoi(arg.substr(7));
        } else if (arg.rfind("--tls-cert=", 0) == 0) {
            cfg.server.tlsCertFile = arg.substr(11);
            cfg.server.tlsEnabled = true;
        } else if (arg.rfind("--tls-key=", 0) == 0) {
            cfg.server.tlsKeyFile = arg.substr(10);
            cfg.server.tlsEnabled = true;
        } else if (arg.rfind("--tls-ca=", 0) == 0) {
            cfg.server.tlsCaFile = arg.substr(9);
        } else if (arg == "--inspect") {
            cfg.inspectorEnabled = true;
        } else if (arg == "--inspect-brk") {
            cfg.inspectorEnabled = true;
            cfg.inspectorBreak = true;
        } else if (arg.rfind("--inspect-port=", 0) == 0) {
            cfg.inspectorPort = std::stoi(arg.substr(15));
            cfg.inspectorEnabled = true;
        }
    }

    if (cfg.server.bindAddress == "0.0.0.0" && cfg.server.bearerToken.empty()) {
        LOG_WARN("--remote without --token is insecure. Use --token=SECRET.");
    }

    // Validate TLS configuration
    // TLS yapilandirmasini dogrula
    if (cfg.server.tlsEnabled) {
        if (cfg.server.tlsCertFile.empty() || cfg.server.tlsKeyFile.empty()) {
            LOG_ERROR("TLS requires both --tls-cert= and --tls-key= flags.");
            cfg.server.tlsEnabled = false;
        }
#ifndef BERKIDE_TLS_ENABLED
        LOG_WARN("TLS flags provided but BerkIDE was built without TLS support. Use -DBERKIDE_USE_TLS=ON.");
        cfg.server.tlsEnabled = false;
#endif
    }

    return cfg;
}

// BerkIDE main entry point (headless server)
// BerkIDE ana giris noktasi (basliksiz sunucu)
int main(int argc, char *argv[])
{
    AppConfig appCfg = parseArgs(argc, argv);

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
    autoSave.setDirectory(berkide::BerkidePaths::instance().userBerkide + "/autosave");
    sessionMgr.setSessionPath(berkide::BerkidePaths::instance().userBerkide + "/session.json");
    httpServer.setEditorContext(&edCtx);
    wsServer.setEditorContext(&edCtx);

    try {
        LOG_INFO("BerkIDE v", BERKIDE_VERSION, " starting...");

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
        StartPluginWatcher(eng);

        // Start HTTP + WS servers
        // HTTP + WS sunucularini baslat
        LOG_INFO("Starting servers...");
        httpServer.start(appCfg.server);
        wsServer.start(appCfg.server);

        // Start auto-save background thread
        // Otomatik kaydetme arka plan thread'ini baslat
        autoSave.start();

        LOG_INFO("BerkIDE running. Press Ctrl+C to stop.");

        // Register platform-specific signal handlers
        // Platforma ozgu sinyal isleyicilerini kaydet
#ifdef _WIN32
        SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
#endif

        // Main event loop: wait for shutdown signal, pump inspector messages
        // Ana olay dongusu: kapatma sinyalini bekle, inspector mesajlarini isle
        while (g_running) {
            eng.pumpInspectorMessages();
            workerMgr.processPendingMessages();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // Graceful shutdown: save session, kill child processes, then stop servers
        // Duzgun kapatma: oturumu kaydet, once alt surecleri durdur, sonra sunuculari kapat
        LOG_INFO("Shutting down...");
        workerMgr.terminateAll();
        sessionMgr.save(bufs);
        autoSave.stop();
        procMgr.shutdownAll();
        httpServer.stop();
        wsServer.stop();

        ShutdownEngine(eng);
        LOG_INFO("BerkIDE shut down successfully.");
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error: ", e.what());
    }

    return 0;
}
