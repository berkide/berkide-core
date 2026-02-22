// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <atomic>
#include <filesystem>
#include <thread>
#include <chrono>
#include "v8.h"
#include "libplatform/libplatform.h"
#include "CommandRouter.h"
#include "commands.h"
#include "HttpServer.h"
#include "EventBus.h"
#include "EditorContext.h"
#include "InspectorServer.h"

// Core V8 JavaScript engine wrapper for BerkIDE.
// BerkIDE icin temel V8 JavaScript motoru sarmalayicisi.
// Manages V8 isolate, context, script execution, plugin loading, and hot-reload.
// V8 izolasyonu, baglam, betik yurutme, eklenti yukleme ve anlik yeniden yuklemeyi yonetir.
// Exposes the global 'editor' object to JavaScript with all bindings.
// Tum binding'lerle birlikte global 'editor' nesnesini JavaScript'e acar.
class V8Engine {
public:
    V8Engine();
    ~V8Engine();

    // Initialize V8 platform, isolate, context, and all bindings
    // V8 platformunu, izolasyonu, baglami ve tum binding'leri baslat
    bool initialize();

    // Shutdown V8 and release all resources
    // V8'i kapat ve tum kaynaklari serbest birak
    void shutdown();

    // Set the editor context (connects V8 bindings to real C++ objects)
    // Editor baglamini ayarla (V8 binding'lerini gercek C++ nesnelerine baglar)
    void setEditorContext(EditorContext& ctx) { edCtx_ = &ctx; }

    // Get the editor context pointer
    // Editor baglam isaretcisini al
    EditorContext* editorContext() { return edCtx_; }

    // Execute a JavaScript source string
    // Bir JavaScript kaynak dizesini calistir
    bool runScript(const std::string& source);

    // Load and execute a JavaScript file as ES6 module (import/export works)
    // Bir JavaScript dosyasini ES6 modul olarak yukle ve calistir (import/export calisir)
    bool loadScriptFromFile(const std::string& path);

    // Load an ES6 module using V8 Module API (works for both .js and .mjs)
    // V8 Module API kullanarak bir ES6 modulu yukle (.js ve .mjs icin calisir)
    bool loadModule(const std::string& path);

    // Legacy: Load a JS file wrapped in IIFE (no import/export, kept as fallback)
    // Eski yontem: Bir JS dosyasini IIFE ile sarili yukle (import/export yok, yedek olarak saklanir)
    bool loadScriptAsIIFE(const std::string& path);

    // Load all .js/.mjs files from a directory as ES6 modules (init.js loads first)
    // Bir dizindeki tum .js/.mjs dosyalarini ES6 modul olarak yukle (init.js ilk yuklenir)
    bool loadAllScripts(const std::string& dirPath, bool recursive = true);

    // Rebuild the global 'editor' object with all bindings (hot-reload)
    // Tum binding'lerle global 'editor' nesnesini yeniden kur (anlik yeniden yukleme)
    void reloadAllBindings();

    // Reload a single binding by name
    // Ismiyle tek bir binding'i yeniden yukle
    bool reloadBinding(const std::string& name);



    // Access the command router for registering/executing commands
    // Komut kaydetme/calistirma icin komut yonlendiricisine eris
    CommandRouter& commandRouter() { return *router_; }

    // Dispatch a command by name (tries C++ first, then JS), returns JSON result
    // Ismiyle bir komut dagit (once C++, sonra JS dener), JSON sonuc dondurur
    json dispatchCommand(const std::string& name, const nlohmann::json& args);

    // List all registered commands and queries
    // Tum kayitli komutlari ve sorgulari listele
    json listCommands() const;

    // Resolve a module/file specifier to an absolute path relative to the referrer
    // Modul/dosya belirleyicisini referrer'a goreceli mutlak yola cozumle
    static std::string resolveModulePath(const std::string& specifier, const std::string& referrerPath);

    // Global singleton access
    // Global tekil erisim
    static V8Engine& instance();

    // Access the internal event bus (for C++ modules)
    // Dahili olay veri yoluna eris (C++ modulleri icin)
    EventBus& eventBus() { return eventBus_; }

    // Access V8 platform (for foreground task runner scheduling)
    // V8 platformuna eris (on plan gorev calistiricisi zamanlama icin)
    v8::Platform* platform() { return platform_.get(); }

    // Start V8 Inspector for Chrome DevTools debugging
    // Chrome DevTools hata ayiklama icin V8 Inspector'u baslat
    bool startInspector(int port = 9229, bool breakOnStart = false);

    // Stop V8 Inspector
    // V8 Inspector'u durdur
    void stopInspector();

    // Process pending inspector messages (call from main thread)
    // Bekleyen inspector mesajlarini isle (ana thread'den cagir)
    void pumpInspectorMessages();

private:
    // Inject console.log into the V8 context
    // V8 baglamina console.log enjekte et
    void injectConsole(v8::Local<v8::Context> ctx);

    // Read a file's contents as a string
    // Bir dosyanin icerigini dize olarak oku
    static std::string readFile(const std::string& path);

    // Rebuild the global 'editor' object (delete and recreate with all bindings)
    // Global 'editor' nesnesini yeniden kur (sil ve tum binding'lerle yeniden olustur)
    void rebuildEditorObject(v8::Local<v8::Context> ctx);

    // Inject setTimeout/clearTimeout into the V8 context
    // V8 baglamina setTimeout/clearTimeout enjekte et
    void injectTimers(v8::Local<v8::Context> ctx);

    // V8 platform and isolate
    // V8 platformu ve izolasyonu
    std::unique_ptr<v8::Platform> platform_;
    v8::ArrayBuffer::Allocator* allocator_ = nullptr;
    v8::Isolate* isolate_ = nullptr;
    v8::Global<v8::Context> context_;

    // Command system
    // Komut sistemi
    std::unique_ptr<CommandRouter> router_;
    HttpServer httpServer_;

    // Internal event bus (V8Engine's own, not EditorContext's)
    // Dahili olay veri yolu (V8Engine'in kendi, EditorContext'inki degil)
    EventBus eventBus_;

    // Editor context pointer (set by main.cpp)
    // Editor baglam isaretcisi (main.cpp tarafindan ayarlanir)
    EditorContext* edCtx_ = nullptr;

    // Plugin tag for console output (empty = plain [JS], non-empty = appended after [JS])
    // Console ciktisi icin plugin etiketi (bos = duz [JS], dolu = [JS] sonrasina eklenir)
    std::string pluginTag_;
public:
    void setPluginTag(const std::string& tag) { pluginTag_ = tag; }
    const std::string& pluginTag() const { return pluginTag_; }
private:

    // Timer management for setTimeout/clearTimeout
    // setTimeout/clearTimeout icin zamanlayici yonetimi
    struct TimerData {
        std::atomic<bool> cancelled{false};
    };
    std::mutex timerMutex_;
    std::unordered_map<int, std::shared_ptr<TimerData>> timerMap_;
    std::atomic<int> timerIdCounter_{1};

    // ES6 Module system
    // ES6 Modul sistemi
    static v8::MaybeLocal<v8::Module> resolveModuleCallback(
        v8::Local<v8::Context> context, v8::Local<v8::String> specifier,
        v8::Local<v8::FixedArray> import_assertions, v8::Local<v8::Module> referrer);
    v8::MaybeLocal<v8::Module> compileModule(const std::string& path, const std::string& source);
    std::unordered_map<std::string, v8::Global<v8::Module>> moduleCache_;
    std::unordered_map<int, std::string> moduleIdToPath_;

    // V8 Inspector for Chrome DevTools debugging
    // Chrome DevTools hata ayiklama icin V8 Inspector
    std::unique_ptr<InspectorServer> inspector_;
};
