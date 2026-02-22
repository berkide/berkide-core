// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "ConfigBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "Config.h"
#include "V8ResponseBuilder.h"
#include <v8.h>
#include <sstream>

// Register editor.config JS object with read-only config access
// Salt-okunur config erisimiyle editor.config JS nesnesini kaydet
void RegisterConfigBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& /*ctx*/) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsConfig = v8::Object::New(isolate);

    // config.get(key) -> value: get a config value by dot-notation key
    // Nokta notasyonu anahtariyla config degeri al
    jsConfig->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "get"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* iso = args.GetIsolate();
            if (args.Length() < 1) {
                args.GetReturnValue().SetUndefined();
                return;
            }

            v8::String::Utf8Value keyStr(iso, args[0]);
            if (!*keyStr) {
                args.GetReturnValue().SetUndefined();
                return;
            }
            std::string key = *keyStr;

            const Config& cfg = Config::instance();
            json raw = cfg.raw();

            // Resolve dot-notation key manually
            // Nokta notasyonu anahtarini manuel cozumle
            const json* node = &raw;
            std::istringstream ss(key);
            std::string part;
            while (std::getline(ss, part, '.')) {
                if (!node->is_object() || !node->contains(part)) {
                    args.GetReturnValue().SetUndefined();
                    return;
                }
                node = &(*node)[part];
            }

            auto ctx = iso->GetCurrentContext();
            args.GetReturnValue().Set(V8Response::jsonToV8(iso, ctx, *node));
        }).ToLocalChecked()
    ).Check();

    // config.getAll() -> object: get the entire config as a JS object
    // Tum config'i JS nesnesi olarak al
    jsConfig->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            const Config& cfg = Config::instance();
            json raw = cfg.raw();
            args.GetReturnValue().Set(V8Response::jsonToV8(iso, ctx, raw));
        }).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx, v8::String::NewFromUtf8Literal(isolate, "config"), jsConfig).Check();
}

// Auto-register "config" binding at static init time
// "config" binding'ini statik baslangicta otomatik kaydet
static bool _configReg = [] {
    BindingRegistry::instance().registerBinding("config",
        [](v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
            RegisterConfigBinding(isolate, editorObj, ctx);
        });
    return true;
}();
