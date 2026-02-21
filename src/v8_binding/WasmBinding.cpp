#include "WasmBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "Logger.h"
#include <v8.h>
#include <fstream>
#include <vector>

// Read a binary file into a byte vector
// Bir ikili dosyayi bayt vektorune oku
static std::vector<uint8_t> readBinaryFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return {};
    auto size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Register editor.wasm JS binding
// editor.wasm JS binding'ini kaydet
void RegisterWasmBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto context = isolate->GetCurrentContext();
    auto wasmObj = v8::Object::New(isolate);

    // editor.wasm.isSupported() -> bool
    // V8 always supports WebAssembly, but check runtime flag
    // V8 her zaman WebAssembly destekler, ama calisma zamani bayragini kontrol et
    wasmObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "isSupported"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();

            // Check if WebAssembly global exists
            // WebAssembly global'inin var olup olmadigini kontrol et
            v8::Local<v8::Value> wasmGlobal;
            bool supported = ctx2->Global()->Get(ctx2,
                v8::String::NewFromUtf8Literal(iso, "WebAssembly")).ToLocal(&wasmGlobal)
                && wasmGlobal->IsObject();
            args.GetReturnValue().Set(supported);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.wasm.loadFile(path) -> WebAssembly.Module
    // Reads a .wasm file, compiles it into a WebAssembly.Module
    // Bir .wasm dosyasini okur, WebAssembly.Module'e derler
    wasmObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "loadFile"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();

            if (args.Length() < 1) {
                iso->ThrowException(v8::Exception::TypeError(
                    v8::String::NewFromUtf8Literal(iso, "loadFile requires a file path")));
                return;
            }

            std::string path = v8Str(iso, args[0]);
            std::vector<uint8_t> bytes = readBinaryFile(path);
            if (bytes.empty()) {
                iso->ThrowException(v8::Exception::Error(
                    v8::String::NewFromUtf8(iso,
                        ("Failed to read WASM file: " + path).c_str()).ToLocalChecked()));
                return;
            }

            // Create ArrayBuffer from the bytes
            // Baytlardan ArrayBuffer olustur
            auto backingStore = v8::ArrayBuffer::NewBackingStore(
                bytes.data(), bytes.size(),
                [](void* data, size_t, void* hint) {
                    delete static_cast<std::vector<uint8_t>*>(hint);
                },
                new std::vector<uint8_t>(std::move(bytes)));

            v8::Local<v8::ArrayBuffer> arrayBuf = v8::ArrayBuffer::New(iso, std::move(backingStore));

            // Call WebAssembly.compile(arrayBuffer) synchronously via WebAssembly.Module constructor
            // WebAssembly.Module yapilandiriciyla WebAssembly.compile(arrayBuffer) senkron olarak cagir
            v8::Local<v8::Value> wasmGlobal;
            if (!ctx2->Global()->Get(ctx2,
                    v8::String::NewFromUtf8Literal(iso, "WebAssembly")).ToLocal(&wasmGlobal)
                || !wasmGlobal->IsObject()) {
                iso->ThrowException(v8::Exception::Error(
                    v8::String::NewFromUtf8Literal(iso, "WebAssembly not available")));
                return;
            }

            // Get WebAssembly.Module constructor
            // WebAssembly.Module yapilandiriciyi al
            v8::Local<v8::Object> wasmNs = wasmGlobal.As<v8::Object>();
            v8::Local<v8::Value> moduleCtor;
            if (!wasmNs->Get(ctx2,
                    v8::String::NewFromUtf8Literal(iso, "Module")).ToLocal(&moduleCtor)
                || !moduleCtor->IsFunction()) {
                iso->ThrowException(v8::Exception::Error(
                    v8::String::NewFromUtf8Literal(iso, "WebAssembly.Module not available")));
                return;
            }

            // new WebAssembly.Module(arrayBuffer)
            v8::Local<v8::Value> ctorArgs[] = { arrayBuf };
            v8::TryCatch tryCatch(iso);
            v8::Local<v8::Value> module;
            if (!moduleCtor.As<v8::Function>()->NewInstance(ctx2, 1, ctorArgs).ToLocal(&module)) {
                if (tryCatch.HasCaught()) {
                    tryCatch.ReThrow();
                }
                return;
            }

            args.GetReturnValue().Set(module);
            LOG_INFO("[WASM] Loaded module from: ", path);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.wasm.instantiate(module, imports?) -> WebAssembly.Instance
    // Instantiate a WebAssembly.Module with optional imports
    // Istege bagli import'larla bir WebAssembly.Module ornekle
    wasmObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "instantiate"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();

            if (args.Length() < 1) {
                iso->ThrowException(v8::Exception::TypeError(
                    v8::String::NewFromUtf8Literal(iso, "instantiate requires a module")));
                return;
            }

            // Get WebAssembly.Instance constructor
            // WebAssembly.Instance yapilandiriciyi al
            v8::Local<v8::Value> wasmGlobal;
            if (!ctx2->Global()->Get(ctx2,
                    v8::String::NewFromUtf8Literal(iso, "WebAssembly")).ToLocal(&wasmGlobal)
                || !wasmGlobal->IsObject()) {
                iso->ThrowException(v8::Exception::Error(
                    v8::String::NewFromUtf8Literal(iso, "WebAssembly not available")));
                return;
            }

            v8::Local<v8::Object> wasmNs = wasmGlobal.As<v8::Object>();
            v8::Local<v8::Value> instanceCtor;
            if (!wasmNs->Get(ctx2,
                    v8::String::NewFromUtf8Literal(iso, "Instance")).ToLocal(&instanceCtor)
                || !instanceCtor->IsFunction()) {
                iso->ThrowException(v8::Exception::Error(
                    v8::String::NewFromUtf8Literal(iso, "WebAssembly.Instance not available")));
                return;
            }

            // new WebAssembly.Instance(module, imports?)
            v8::TryCatch tryCatch(iso);
            v8::Local<v8::Value> instance;
            if (args.Length() >= 2 && args[1]->IsObject()) {
                v8::Local<v8::Value> ctorArgs[] = { args[0], args[1] };
                if (!instanceCtor.As<v8::Function>()->NewInstance(ctx2, 2, ctorArgs).ToLocal(&instance)) {
                    if (tryCatch.HasCaught()) tryCatch.ReThrow();
                    return;
                }
            } else {
                v8::Local<v8::Value> ctorArgs[] = { args[0] };
                if (!instanceCtor.As<v8::Function>()->NewInstance(ctx2, 1, ctorArgs).ToLocal(&instance)) {
                    if (tryCatch.HasCaught()) tryCatch.ReThrow();
                    return;
                }
            }

            args.GetReturnValue().Set(instance);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "wasm"),
        wasmObj).Check();
}

// Self-register at static initialization time
// Statik baslatma zamaninda kendini kaydet
static bool _wasmReg = [] {
    BindingRegistry::instance().registerBinding("wasm", RegisterWasmBinding);
    return true;
}();
