// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "WasmBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
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

// Context struct to pass i18n to lambda callbacks
// Lambda callback'lere i18n isaretcisini aktarmak icin baglam yapisi
struct WasmCtx {
    I18n* i18n;
};

// Register editor.wasm JS binding
// editor.wasm JS binding'ini kaydet
void RegisterWasmBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto context = isolate->GetCurrentContext();
    auto wasmObj = v8::Object::New(isolate);

    auto* wctx = new WasmCtx{ctx.i18n};

    // editor.wasm.isSupported() -> {ok, data: bool, ...}
    // V8 always supports WebAssembly, but check runtime flag
    // V8 her zaman WebAssembly destekler, ama calisma zamani bayragini kontrol et
    wasmObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "isSupported"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WasmCtx*>(args.Data().As<v8::External>()->Value());
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();

            // Check if WebAssembly global exists
            // WebAssembly global'inin var olup olmadigini kontrol et
            v8::Local<v8::Value> wasmGlobal;
            bool supported = ctx2->Global()->Get(ctx2,
                v8::String::NewFromUtf8Literal(iso, "WebAssembly")).ToLocal(&wasmGlobal)
                && wasmGlobal->IsObject();
            V8Response::ok(args, supported);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // editor.wasm.loadFile(path) -> WebAssembly.Module (raw V8 object, not wrapped)
    // Reads a .wasm file, compiles it into a WebAssembly.Module
    // Bir .wasm dosyasini okur, WebAssembly.Module'e derler
    // NOTE: Returns raw V8 Module object since it must be used with WebAssembly.Instance constructor
    // NOT: WebAssembly.Instance yapilandiriciyla kullanilmasi gerektigi icin ham V8 Module nesnesi dondurur
    wasmObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "loadFile"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WasmCtx*>(args.Data().As<v8::External>()->Value());
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();

            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "path"}}, wc ? wc->i18n : nullptr);
                return;
            }

            std::string path = v8Str(iso, args[0]);
            std::vector<uint8_t> bytes = readBinaryFile(path);
            if (bytes.empty()) {
                V8Response::error(args, "LOAD_ERROR", "wasm.loadfile.error",
                    {{"path", path}}, wc ? wc->i18n : nullptr);
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
                V8Response::error(args, "WASM_UNAVAILABLE", "wasm.not_available", {}, wc ? wc->i18n : nullptr);
                return;
            }

            // Get WebAssembly.Module constructor
            // WebAssembly.Module yapilandiriciyi al
            v8::Local<v8::Object> wasmNs = wasmGlobal.As<v8::Object>();
            v8::Local<v8::Value> moduleCtor;
            if (!wasmNs->Get(ctx2,
                    v8::String::NewFromUtf8Literal(iso, "Module")).ToLocal(&moduleCtor)
                || !moduleCtor->IsFunction()) {
                V8Response::error(args, "WASM_UNAVAILABLE", "wasm.module_not_available", {}, wc ? wc->i18n : nullptr);
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

            // Return raw V8 Module object (not wrapped in standard response)
            // Ham V8 Module nesnesini dondur (standart yanita sarmalanmamis)
            args.GetReturnValue().Set(module);
            LOG_INFO("[WASM] Loaded module from: ", path);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // editor.wasm.instantiate(module, imports?) -> WebAssembly.Instance (raw V8 object)
    // Instantiate a WebAssembly.Module with optional imports
    // Istege bagli import'larla bir WebAssembly.Module ornekle
    // NOTE: Returns raw V8 Instance object since it exposes exported functions directly
    // NOT: Disa aktarilan fonksiyonlari dogrudan sundugundan ham V8 Instance nesnesi dondurur
    wasmObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "instantiate"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WasmCtx*>(args.Data().As<v8::External>()->Value());
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();

            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "module"}}, wc ? wc->i18n : nullptr);
                return;
            }

            // Get WebAssembly.Instance constructor
            // WebAssembly.Instance yapilandiriciyi al
            v8::Local<v8::Value> wasmGlobal;
            if (!ctx2->Global()->Get(ctx2,
                    v8::String::NewFromUtf8Literal(iso, "WebAssembly")).ToLocal(&wasmGlobal)
                || !wasmGlobal->IsObject()) {
                V8Response::error(args, "WASM_UNAVAILABLE", "wasm.not_available", {}, wc ? wc->i18n : nullptr);
                return;
            }

            v8::Local<v8::Object> wasmNs = wasmGlobal.As<v8::Object>();
            v8::Local<v8::Value> instanceCtor;
            if (!wasmNs->Get(ctx2,
                    v8::String::NewFromUtf8Literal(iso, "Instance")).ToLocal(&instanceCtor)
                || !instanceCtor->IsFunction()) {
                V8Response::error(args, "WASM_UNAVAILABLE", "wasm.instance_not_available", {}, wc ? wc->i18n : nullptr);
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

            // Return raw V8 Instance object (not wrapped in standard response)
            // Ham V8 Instance nesnesini dondur (standart yanita sarmalanmamis)
            args.GetReturnValue().Set(instance);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
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
