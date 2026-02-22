// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "V8Engine.h"
#include "EditorBinding.h"
#include "BindingRegistry.h"
#include "HttpServer.h"
#include "Logger.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

// Constructor: create the command router for JS-to-native command dispatch
// Constructor: JS-native komut yonlendirmesi icin CommandRouter olustur
V8Engine::V8Engine()
{
    router_ = std::make_unique<CommandRouter>();
}
V8Engine::~V8Engine() { shutdown(); }

// Initialize V8 engine: platform, isolate, context, inject console/timers, bind editor + commands
// V8 motorunu baslat: platform, isolate, context olustur, console/timer ekle, editor + komut bagla
bool V8Engine::initialize()
{
    // Set up V8 platform and create isolate with default allocator
    // V8 platformunu kur ve varsayilan bellek ayirici ile isolate olustur
    v8::V8::InitializeICUDefaultLocation(nullptr);
    v8::V8::InitializeExternalStartupData(nullptr);

    platform_ = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform_.get());
    v8::V8::Initialize();

    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = (allocator_ = v8::ArrayBuffer::Allocator::NewDefaultAllocator());

    isolate_ = v8::Isolate::New(create_params);
    isolate_->SetData(0, this);
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);

    v8::Local<v8::Context> ctx = v8::Context::New(isolate_);
    v8::Context::Scope context_scope(ctx);

    // Inject console.log and setTimeout/clearTimeout into global scope
    // console.log ve setTimeout/clearTimeout fonksiyonlarini global scope'a ekle
    injectConsole(ctx);
    injectTimers(ctx);

    // Create editor JS object and apply all registered bindings
    // editor JS nesnesini olustur ve tum kayitli binding'leri uygula
    if (edCtx_) {
        BindEditor(isolate_, ctx, *edCtx_);
    } else {
        // Fallback: create a dummy context (bindings will have null pointers)
        EditorContext dummy;
        BindEditor(isolate_, ctx, dummy);
    }

    // Register built-in native commands (move, insert, delete, etc.)
    // Yerlesik native komutlari kaydet (move, insert, delete, vb.)
    RegisterCommands(*router_, edCtx_);

    LOG_INFO("[V8] EventBus initialized & bridged");

    context_.Reset(isolate_, ctx);
    LOG_INFO("[V8] Engine initialized");
    return true;
}

// Shut down V8 engine: stop HTTP server, event bus, dispose isolate and platform
// V8 motorunu kapat: HTTP sunucusu, event bus'i durdur, isolate ve platformu serbest birak
void V8Engine::shutdown()
{
    // Prevent double shutdown (destructor may call this again after explicit shutdown)
    // Cift kapatmayi onle (yikici, acik kapatmadan sonra bunu tekrar cagirabilir)
    if (!isolate_)
        return;

    if (httpServer_.isRunning())
    {
        httpServer_.stop();
    }

    // Stop inspector before shutting down V8
    // V8'i kapatmadan once inspector'u durdur
    stopInspector();

    eventBus_.shutdown();

    // Clear all cached V8 handles before disposing isolate
    // Isolate'i elden cikarmadan once tum onbelleklenmis V8 handle'larini temizle
    for (auto& [path, handle] : moduleCache_) {
        handle.Reset();
    }
    moduleCache_.clear();
    moduleIdToPath_.clear();
    context_.Reset();

    isolate_->Dispose();
    delete allocator_;
    isolate_ = nullptr;
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
    LOG_INFO("[V8] Engine shutdown");
}

// Compile and run a JavaScript source string in the current V8 context
// Mevcut V8 context icinde bir JavaScript kaynak kodunu derle ve calistir
bool V8Engine::runScript(const std::string &source)
{
    if (!isolate_)
        return false;

    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    auto ctx = context_.Get(isolate_);
    v8::Context::Scope context_scope(ctx);

    v8::TryCatch trycatch(isolate_);
    v8::Local<v8::String> code =
        v8::String::NewFromUtf8(isolate_, source.c_str()).ToLocalChecked();

    v8::Local<v8::Script> script;
    if (!v8::Script::Compile(ctx, code).ToLocal(&script))
    {
        v8::String::Utf8Value err(isolate_, trycatch.Exception());
        LOG_ERROR("[V8] Compile error: ", *err);
        return false;
    }

    v8::Local<v8::Value> result;
    if (!script->Run(ctx).ToLocal(&result))
    {
        v8::String::Utf8Value err(isolate_, trycatch.Exception());
        LOG_ERROR("[V8] Runtime error: ", *err);
        return false;
    }

    return true;
}

// Load a JS file as an ES6 module (import/export supported)
// Bir JS dosyasini ES6 modul olarak yukle (import/export desteklenir)
bool V8Engine::loadScriptFromFile(const std::string &path)
{
    // All .js files are now loaded as ES6 modules — no IIFE wrapping.
    // Tum .js dosyalari artik ES6 modul olarak yuklenir — IIFE sarmalama yok.
    return loadModule(path);
}

// Legacy: Load a JS file wrapped in IIFE (no import/export, isolated scope)
// Eski yontem: Bir JS dosyasini IIFE ile sarili yukle (import/export yok, izole kapsam)
// Kept as fallback for edge cases. Prefer loadScriptFromFile() or loadModule().
// Uç durumlar icin yedek olarak saklanir. loadScriptFromFile() veya loadModule() tercih edin.
bool V8Engine::loadScriptAsIIFE(const std::string &path)
{
    if (!isolate_)
        return false;

    std::ifstream file(path);
    if (!file.is_open())
    {
        LOG_ERROR("[V8] File read failed: ", path);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string src = buffer.str();

    v8::Locker locker(isolate_);
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    auto context = context_.Get(isolate_);
    v8::Context::Scope context_scope(context);

    std::string wrapped = "(function(){\n" + src + "\n})();";

    v8::Local<v8::String> code =
        v8::String::NewFromUtf8(isolate_, wrapped.c_str(), v8::NewStringType::kNormal)
            .ToLocalChecked();

    v8::Local<v8::Script> script;
    if (!v8::Script::Compile(context, code).ToLocal(&script))
    {
        LOG_ERROR("[V8] Script compile error in ", path);
        return false;
    }

    v8::TryCatch try_catch(isolate_);
    v8::MaybeLocal<v8::Value> result = script->Run(context);
    if (result.IsEmpty())
    {
        v8::String::Utf8Value error(isolate_, try_catch.Exception());
        LOG_ERROR("[V8] Runtime error: ", *error, " (", path, ")");
        return false;
    }

    LOG_INFO("[V8] File executed (IIFE): ", path);
    return true;
}
// Resolve a module specifier to an absolute file path
// Modul belirleyicisini mutlak dosya yoluna coz
std::string V8Engine::resolveModulePath(const std::string& specifier, const std::string& referrerPath) {
    // @berkide/ prefix -> ~/.berkide/
    // @berkide/ oneki -> ~/.berkide/
    if (specifier.rfind("@berkide/", 0) == 0) {
        const char* home = std::getenv("HOME");
#ifdef _WIN32
        if (!home) home = std::getenv("USERPROFILE");
#endif
        if (home) {
            std::string resolved = std::string(home) + "/.berkide/" + specifier.substr(9);
            // Probe extensions if no extension given
            // Uzanti verilmemisse uzantilari dene
            if (fs::exists(resolved)) return fs::canonical(resolved).string();
            if (fs::exists(resolved + ".mjs")) return fs::canonical(resolved + ".mjs").string();
            if (fs::exists(resolved + ".js")) return fs::canonical(resolved + ".js").string();
            if (fs::exists(resolved + "/index.mjs")) return fs::canonical(resolved + "/index.mjs").string();
            if (fs::exists(resolved + "/index.js")) return fs::canonical(resolved + "/index.js").string();
            return resolved;
        }
    }

    // Relative path: resolve from referrer's directory
    // Goreceli yol: referrer'in dizininden coz
    fs::path base = fs::path(referrerPath).parent_path();
    fs::path candidate = base / specifier;

    if (fs::exists(candidate)) return fs::canonical(candidate).string();
    if (fs::exists(candidate.string() + ".mjs")) return fs::canonical(candidate.string() + ".mjs").string();
    if (fs::exists(candidate.string() + ".js")) return fs::canonical(candidate.string() + ".js").string();
    if (fs::exists(candidate / "index.mjs")) return fs::canonical(candidate / "index.mjs").string();
    if (fs::exists(candidate / "index.js")) return fs::canonical(candidate / "index.js").string();

    return candidate.string();
}

// Compile a source string into a V8 Module and cache it
// Kaynak dizesini V8 Module'e derle ve onbellege al
v8::MaybeLocal<v8::Module> V8Engine::compileModule(const std::string& path, const std::string& source) {
    v8::Local<v8::String> v8Source = v8::String::NewFromUtf8(isolate_, source.c_str()).ToLocalChecked();
    v8::Local<v8::String> v8Name = v8::String::NewFromUtf8(isolate_, path.c_str()).ToLocalChecked();

    v8::ScriptOrigin origin(v8Name, 0, 0, false, -1,
                            v8::Local<v8::Value>(), false, false, true /* is_module */);
    v8::ScriptCompiler::Source compileSource(v8Source, origin);

    v8::TryCatch trycatch(isolate_);
    v8::Local<v8::Module> module;
    if (!v8::ScriptCompiler::CompileModule(isolate_, &compileSource).ToLocal(&module)) {
        v8::String::Utf8Value err(isolate_, trycatch.Exception());
        LOG_ERROR("[V8] Module compile error in ", path, ": ", *err);
        return {};
    }

    moduleIdToPath_[module->GetIdentityHash()] = path;
    moduleCache_[path].Reset(isolate_, module);
    return module;
}

// Static callback for V8 module resolution (import statements)
// V8 modul cozumlemesi icin statik geri cagirim (import ifadeleri)
v8::MaybeLocal<v8::Module> V8Engine::resolveModuleCallback(
    v8::Local<v8::Context> context, v8::Local<v8::String> specifier,
    v8::Local<v8::FixedArray> /*import_assertions*/, v8::Local<v8::Module> referrer)
{
    // Get isolate from the engine singleton (context doesn't expose GetIsolate in this V8 version)
    // Isolate'i engine singleton'dan al (bu V8 versiyonunda context GetIsolate'i acmiyor)
    auto& eng = V8Engine::instance();
    v8::Isolate* isolate = eng.isolate_;
    auto* engine = &eng;

    v8::String::Utf8Value specUtf8(isolate, specifier);
    std::string specStr = *specUtf8 ? *specUtf8 : "";

    // Find referrer path from identity hash
    // Referrer yolunu kimlik hash'inden bul
    std::string referrerPath = "";
    auto it = engine->moduleIdToPath_.find(referrer->GetIdentityHash());
    if (it != engine->moduleIdToPath_.end()) referrerPath = it->second;

    std::string resolved = engine->resolveModulePath(specStr, referrerPath);

    // Check cache first
    // Once onbellegi kontrol et
    auto cacheIt = engine->moduleCache_.find(resolved);
    if (cacheIt != engine->moduleCache_.end()) {
        return cacheIt->second.Get(isolate);
    }

    // Read and compile the module
    // Modulu oku ve derle
    std::string source = readFile(resolved);
    if (source.empty()) {
        LOG_ERROR("[V8] Module not found: ", resolved, " (specifier: ", specStr, ")");
        return {};
    }

    return engine->compileModule(resolved, source);
}

// Load an ES6 module file: read, compile, instantiate, and evaluate
// Bir ES6 modul dosyasini yukle: oku, derle, ornekle ve degerlendir
bool V8Engine::loadModule(const std::string& path) {
    if (!isolate_) return false;

    std::string canonical;
    try {
        canonical = fs::canonical(path).string();
    } catch (...) {
        canonical = path;
    }

    // Check cache
    // Onbellegi kontrol et
    if (moduleCache_.find(canonical) != moduleCache_.end()) {
        LOG_DEBUG("[V8] Module already loaded: ", canonical);
        return true;
    }

    std::string source = readFile(canonical);
    if (source.empty()) {
        LOG_ERROR("[V8] Module file read failed: ", canonical);
        return false;
    }

    v8::Locker locker(isolate_);
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    auto ctx = context_.Get(isolate_);
    v8::Context::Scope context_scope(ctx);

    v8::TryCatch trycatch(isolate_);

    v8::Local<v8::Module> module;
    if (!compileModule(canonical, source).ToLocal(&module)) {
        return false;
    }

    if (!module->InstantiateModule(ctx, resolveModuleCallback).FromMaybe(false)) {
        v8::String::Utf8Value err(isolate_, trycatch.Exception());
        LOG_ERROR("[V8] Module instantiate error: ", canonical, " - ", *err);
        return false;
    }

    v8::Local<v8::Value> result;
    if (!module->Evaluate(ctx).ToLocal(&result)) {
        v8::String::Utf8Value err(isolate_, trycatch.Exception());
        LOG_ERROR("[V8] Module evaluate error: ", canonical, " - ", *err);
        return false;
    }

    LOG_INFO("[V8] Module loaded: ", canonical);
    return true;
}

// Reload all bindings by rebuilding the editor JS object from scratch
// Tum binding'leri editor JS nesnesini sifirdan yeniden olusturarak tekrar yukle
void V8Engine::reloadAllBindings()
{
    if (!isolate_)
        return;
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    auto ctx = context_.Get(isolate_);
    v8::Context::Scope context_scope(ctx);
    rebuildEditorObject(ctx);
    LOG_INFO("[V8] All bindings reapplied");
}

// Load and execute all .js/.mjs files from a directory as ES6 modules
// Bir dizindeki tum .js/.mjs dosyalarini ES6 modul olarak yukle ve calistir
// init.js is loaded first if present (like Emacs init.el).
// init.js varsa ilk yuklenir (Emacs init.el gibi).
bool V8Engine::loadAllScripts(const std::string &dirPath, bool recursive)
{
    if (!isolate_)
        return false;
    if (!std::filesystem::exists(dirPath))
        return false;

    std::vector<std::string> files;

    auto collect = [&](auto& iterator) {
        for (auto &e : iterator) {
            if (!e.is_regular_file()) continue;
            auto ext = e.path().extension().string();
            if (ext == ".js" || ext == ".mjs") {
                files.push_back(e.path().string());
            }
        }
    };

    if (recursive) {
        auto it = std::filesystem::recursive_directory_iterator(dirPath);
        collect(it);
    } else {
        auto it = std::filesystem::directory_iterator(dirPath);
        collect(it);
    }

    std::sort(files.begin(), files.end());

    // Prioritize init.js / init.mjs — load first (like Emacs init.el)
    // init.js / init.mjs oncelikli — ilk yukle (Emacs init.el gibi)
    auto initIt = std::stable_partition(files.begin(), files.end(), [](const std::string& f) {
        auto name = std::filesystem::path(f).filename().string();
        return name == "init.js" || name == "init.mjs";
    });

    int count = 0;

    // All files (.js and .mjs) are loaded as ES6 modules
    // Tum dosyalar (.js ve .mjs) ES6 modul olarak yuklenir
    for (auto &f : files) {
        if (loadModule(f)) ++count;
    }

    LOG_INFO("[V8] ", count, " modules loaded (", dirPath, ")");
    return count > 0;
}
// Reload a single named binding (e.g. "buffer", "cursor") on the editor JS object
// editor JS nesnesi uzerinde tek bir binding'i (orn. "buffer", "cursor") yeniden yukle
bool V8Engine::reloadBinding(const std::string &name)
{
    if (!isolate_)
        return false;
    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    auto ctx = context_.Get(isolate_);
    v8::Context::Scope context_scope(ctx);

    auto maybeEditor = ctx->Global()->Get(ctx, v8::String::NewFromUtf8Literal(isolate_, "editor"));
    if (maybeEditor.IsEmpty())
        return false;
    auto editorObj = maybeEditor.ToLocalChecked().As<v8::Object>();

    editorObj->Set(ctx,
                   v8::String::NewFromUtf8(isolate_, name.c_str()).ToLocalChecked(),
                   v8::Undefined(isolate_))
        .Check();

    if (!edCtx_) return false;
    const bool ok = BindingRegistry::instance().applyOne(name, isolate_, editorObj, *edCtx_);
    if (ok)
        LOG_INFO("[V8] Binding reloaded: ", name);
    else
        LOG_WARN("[V8] Binding not found: ", name);
    return ok;
}

// Collect JS arguments into a single string
// JS argumanlarini tek bir dizede topla
static std::string collectJsArgs(const v8::FunctionCallbackInfo<v8::Value>& args) {
    std::string msg;
    for (int i = 0; i < args.Length(); ++i) {
        v8::String::Utf8Value s(args.GetIsolate(), args[i]);
        if (i > 0) msg += " ";
        msg += (*s ? *s : "(null)");
    }
    return msg;
}

// Inject console.log/warn/error/debug into JS global scope, routed through Logger
// JS global scope'a console.log/warn/error/debug ekle, Logger uzerinden yonlendir
void V8Engine::injectConsole(v8::Local<v8::Context> ctx)
{
    auto isolate = isolate_;
    v8::Local<v8::Object> console = v8::Object::New(isolate);

    // console.log -> LOG_INFO
    console->Set(ctx,
        v8::String::NewFromUtf8Literal(isolate, "log"),
        v8::Function::New(ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* eng = static_cast<V8Engine*>(args.GetIsolate()->GetData(0));
            LOG_INFO("[JS] ", eng->pluginTag().empty() ? "" : eng->pluginTag() + "  ", collectJsArgs(args));
        }).ToLocalChecked()
    ).Check();

    // console.warn -> LOG_WARN
    console->Set(ctx,
        v8::String::NewFromUtf8Literal(isolate, "warn"),
        v8::Function::New(ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* eng = static_cast<V8Engine*>(args.GetIsolate()->GetData(0));
            LOG_WARN("[JS] ", eng->pluginTag().empty() ? "" : eng->pluginTag() + "  ", collectJsArgs(args));
        }).ToLocalChecked()
    ).Check();

    // console.error -> LOG_ERROR
    console->Set(ctx,
        v8::String::NewFromUtf8Literal(isolate, "error"),
        v8::Function::New(ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* eng = static_cast<V8Engine*>(args.GetIsolate()->GetData(0));
            LOG_ERROR("[JS] ", eng->pluginTag().empty() ? "" : eng->pluginTag() + "  ", collectJsArgs(args));
        }).ToLocalChecked()
    ).Check();

    // console.debug -> LOG_DEBUG
    console->Set(ctx,
        v8::String::NewFromUtf8Literal(isolate, "debug"),
        v8::Function::New(ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* eng = static_cast<V8Engine*>(args.GetIsolate()->GetData(0));
            LOG_DEBUG("[JS] ", eng->pluginTag().empty() ? "" : eng->pluginTag() + "  ", collectJsArgs(args));
        }).ToLocalChecked()
    ).Check();

    ctx->Global()->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "console"), console).Check();
}

// Delete and recreate the global editor JS object with all bindings re-applied
// Global editor JS nesnesini sil ve tum binding'leri yeniden uygulayarak tekrar olustur
void V8Engine::rebuildEditorObject(v8::Local<v8::Context> ctx)
{
    ctx->Global()->Delete(ctx, v8::String::NewFromUtf8Literal(isolate_, "editor")).Check();
    if (edCtx_) {
        BindEditor(isolate_, ctx, *edCtx_);
    } else {
        EditorContext dummy;
        BindEditor(isolate_, ctx, dummy);
    }
}

// Read entire file contents into a string (binary-safe)
// Dosya iceriginin tamamini bir string'e oku (binary-guvenli)
std::string V8Engine::readFile(const std::string &path)
{
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs)
        return {};
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

// Dispatch a command: try native router first, then fall back to JS editor.commands.exec
// Komutu yonlendir: once native router'i dene, bulunamazsa JS editor.commands.exec'e dusur
json V8Engine::dispatchCommand(const std::string &name, const nlohmann::json &args)
{
    // Try native C++ commands and queries first
    // Once yerel C++ komutlarini ve sorgularini dene
    json result = router_->executeWithResult(name, args);
    if (result.value("ok", false) || result.value("error", "") != "command not found: " + name) {
        return result;
    }

    // Fall back to JS editor.commands.exec
    // JS editor.commands.exec'e dusur
    if (!isolate_)
        return json({{"ok", false}, {"error", "V8 not initialized"}});

    v8::Isolate::Scope isolate_scope(isolate_);
    v8::HandleScope handle_scope(isolate_);
    auto ctx = context_.Get(isolate_);
    v8::Context::Scope context_scope(ctx);

    v8::Local<v8::Object> global = ctx->Global();
    auto maybeEditor = global->Get(ctx, v8::String::NewFromUtf8Literal(isolate_, "editor"));
    if (maybeEditor.IsEmpty())
        return json({{"ok", false}, {"error", "editor object not found"}});

    v8::Local<v8::Object> editorObj = maybeEditor.ToLocalChecked().As<v8::Object>();
    auto maybeCommands = editorObj->Get(ctx, v8::String::NewFromUtf8Literal(isolate_, "commands"));
    if (maybeCommands.IsEmpty())
        return json({{"ok", false}, {"error", "commands object not found"}});

    v8::Local<v8::Object> commandsObj = maybeCommands.ToLocalChecked().As<v8::Object>();
    auto maybeExec = commandsObj->Get(ctx, v8::String::NewFromUtf8Literal(isolate_, "exec"));

    v8::Local<v8::Value> execVal;
    if (!maybeExec.ToLocal(&execVal) || !execVal->IsFunction())
        return json({{"ok", false}, {"error", "JS command not found: " + name}});

    v8::Local<v8::Function> execFn = execVal.As<v8::Function>();

    // Parse args JSON string into a JS object so handlers receive proper objects
    // Args JSON dizesini JS nesnesine ayristir, boylece isleyiciler duzgun nesne alir
    v8::Local<v8::Value> argsVal;
    std::string argsStr = args.dump();
    v8::Local<v8::String> argsJsonStr = v8::String::NewFromUtf8(isolate_, argsStr.c_str()).ToLocalChecked();
    v8::MaybeLocal<v8::Value> parsedArgs = v8::JSON::Parse(ctx, argsJsonStr);
    if (parsedArgs.ToLocal(&argsVal)) {
        // Successfully parsed JSON to object
    } else {
        argsVal = argsJsonStr; // fallback to string
    }

    v8::Local<v8::Value> argv[2] = {
        v8::String::NewFromUtf8(isolate_, name.c_str()).ToLocalChecked(),
        argsVal};

    v8::TryCatch trycatch(isolate_);
    v8::MaybeLocal<v8::Value> maybeResult = execFn->Call(ctx, commandsObj, 2, argv);

    if (trycatch.HasCaught())
    {
        v8::String::Utf8Value err(isolate_, trycatch.Exception());
        LOG_ERROR("[CommandDispatch] JS exec error: ", *err);
        return json({{"ok", false}, {"error", std::string(*err)}});
    }

    // Convert JS return value to JSON using JSON.stringify
    // JS donus degerini JSON.stringify kullanarak JSON'a donustur
    v8::Local<v8::Value> resultVal;
    if (maybeResult.ToLocal(&resultVal) && !resultVal->IsUndefined() && !resultVal->IsNull()) {
        v8::Local<v8::Object> jsonObj = ctx->Global()->Get(ctx,
            v8::String::NewFromUtf8Literal(isolate_, "JSON")).ToLocalChecked().As<v8::Object>();
        v8::Local<v8::Function> stringify = jsonObj->Get(ctx,
            v8::String::NewFromUtf8Literal(isolate_, "stringify")).ToLocalChecked().As<v8::Function>();
        v8::Local<v8::Value> stringifyArgs[1] = { resultVal };
        v8::MaybeLocal<v8::Value> jsonStr = stringify->Call(ctx, jsonObj, 1, stringifyArgs);

        v8::Local<v8::Value> strVal;
        if (jsonStr.ToLocal(&strVal) && strVal->IsString()) {
            v8::String::Utf8Value utf8(isolate_, strVal);
            if (*utf8) {
                auto parsed = json::parse(*utf8, nullptr, false);
                if (!parsed.is_discarded()) {
                    return json({{"ok", true}, {"result", parsed}});
                }
            }
        }
    }

    return json({{"ok", true}});
}

// List all registered commands and queries from the router
// Router'daki tum kayitli komutlari ve sorgulari listele
json V8Engine::listCommands() const {
    return router_->listAll();
}

// Inject setTimeout and clearTimeout into JS global scope using V8 foreground task runner
// setTimeout ve clearTimeout'u V8 foreground task runner kullanarak JS global scope'a ekle
void V8Engine::injectTimers(v8::Local<v8::Context> ctx) {
    auto isolate = isolate_;
    auto global = ctx->Global();

    // setTimeout(callback, delay): schedule a JS callback after delay ms on V8 foreground thread
    // setTimeout(callback, delay): belirtilen ms sonra JS callback'i V8 on plan thread'inde calistir
    auto setTimeoutFn = v8::Function::New(ctx,
        [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 2 || !args[0]->IsFunction() || !args[1]->IsNumber()) return;
            auto* isolate = args.GetIsolate();
            auto* engine = static_cast<V8Engine*>(isolate->GetData(0));

            v8::Local<v8::Function> cb = args[0].As<v8::Function>();
            int delay = args[1]->Int32Value(isolate->GetCurrentContext()).FromMaybe(0);

            int id = engine->timerIdCounter_.fetch_add(1);
            auto td = std::make_shared<TimerData>();
            {
                std::lock_guard<std::mutex> lk(engine->timerMutex_);
                engine->timerMap_[id] = td;
            }

            auto pcb = std::make_unique<v8::Global<v8::Function>>(isolate, cb);
            auto pctx = std::make_unique<v8::Global<v8::Context>>(isolate, isolate->GetCurrentContext());

            std::thread([engine, id, delay, td,
                         pcb = std::move(pcb),
                         pctx = std::move(pctx)]() mutable {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                if (td->cancelled.load()) return;

                struct TaskImpl : v8::Task {
                    V8Engine* eng;
                    int id;
                    std::shared_ptr<TimerData> td;
                    std::unique_ptr<v8::Global<v8::Function>> cb;
                    std::unique_ptr<v8::Global<v8::Context>> ctx;

                    void Run() override {
                        if (td->cancelled.load()) return;
                        v8::Isolate::Scope iso_scope(eng->isolate_);
                        v8::HandleScope hs(eng->isolate_);
                        auto context = ctx->Get(eng->isolate_);
                        v8::Context::Scope cs(context);
                        auto localCb = cb->Get(eng->isolate_);
                        (void)localCb->Call(context, context->Global(), 0, nullptr);

                        std::lock_guard<std::mutex> lg(eng->timerMutex_);
                        eng->timerMap_.erase(id);
                    }
                };

                auto task = std::make_unique<TaskImpl>();
                task->eng = engine;
                task->id = id;
                task->td = td;
                task->cb = std::move(pcb);
                task->ctx = std::move(pctx);

                engine->platform_->GetForegroundTaskRunner(engine->isolate_)->PostTask(std::move(task));
            }).detach();

            args.GetReturnValue().Set(v8::Integer::New(isolate, id));
        }).ToLocalChecked();

    // clearTimeout(id): cancel a pending timer by marking it as cancelled
    // clearTimeout(id): bekleyen bir zamanlayiciyi iptal edildi olarak isaretle
    auto clearTimeoutFn = v8::Function::New(ctx,
        [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1 || !args[0]->IsInt32()) return;
            auto* isolate = args.GetIsolate();
            auto* engine = static_cast<V8Engine*>(isolate->GetData(0));
            int id = args[0]->Int32Value(isolate->GetCurrentContext()).FromMaybe(0);

            std::lock_guard<std::mutex> lk(engine->timerMutex_);
            auto it = engine->timerMap_.find(id);
            if (it != engine->timerMap_.end()) {
                it->second->cancelled.store(true);
                engine->timerMap_.erase(it);
            }
        }).ToLocalChecked();

    global->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "setTimeout"), setTimeoutFn).Check();
    global->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "clearTimeout"), clearTimeoutFn).Check();

    LOG_INFO("[V8] Timers ready (setTimeout/clearTimeout)");
}



// Start V8 Inspector for Chrome DevTools plugin debugging
// Chrome DevTools eklenti hata ayiklamasi icin V8 Inspector'u baslat
bool V8Engine::startInspector(int port, bool breakOnStart) {
    if (!isolate_) return false;

    v8::Isolate::Scope isolateScope(isolate_);
    v8::HandleScope handleScope(isolate_);
    auto ctx = context_.Get(isolate_);
    v8::Context::Scope contextScope(ctx);

    inspector_ = std::make_unique<InspectorServer>();
    bool ok = inspector_->start(isolate_, ctx, port, breakOnStart);
    if (!ok) {
        inspector_.reset();
        LOG_ERROR("[V8] Failed to start inspector on port ", port);
    }
    return ok;
}

// Stop V8 Inspector
// V8 Inspector'u durdur
void V8Engine::stopInspector() {
    if (inspector_) {
        inspector_->stop();
        inspector_.reset();
    }
}

// Process pending inspector messages (call from main thread event loop)
// Bekleyen inspector mesajlarini isle (ana thread olay dongusunden cagir)
void V8Engine::pumpInspectorMessages() {
    if (inspector_ && inspector_->isRunning()) {
        v8::Isolate::Scope isolateScope(isolate_);
        v8::HandleScope handleScope(isolate_);
        auto ctx = context_.Get(isolate_);
        v8::Context::Scope contextScope(ctx);
        inspector_->pumpMessages();
    }
}

// Singleton accessor for the global V8Engine instance
// Global V8Engine ornegine erisim icin singleton erisimci
// Heap-allocated singleton: intentionally never destroyed to avoid
// static destruction order issues with V8 platform/isolate cleanup.
// Heap-ayrilan tekil: V8 platform/izolasyon temizleme ile statik yikim
// sira sorunlarindan kacinmak icin bilerek asla yok edilmez.
V8Engine &V8Engine::instance()
{
    static V8Engine* instance = new V8Engine();
    return *instance;
}
