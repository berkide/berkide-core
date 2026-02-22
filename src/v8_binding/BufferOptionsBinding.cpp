// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "BufferOptionsBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "BufferOptions.h"
#include <v8.h>

// Helper: Convert JS value to OptionValue variant based on detected type
// Yardimci: JS degerini algilanan tipe gore OptionValue variant'ina donustur
static OptionValue JsToOptionValue(v8::Isolate* iso, v8::Local<v8::Value> val) {
    if (val->IsBoolean()) {
        return val->BooleanValue(iso);
    }
    if (val->IsInt32()) {
        return val->Int32Value(iso->GetCurrentContext()).FromJust();
    }
    if (val->IsNumber()) {
        return val->NumberValue(iso->GetCurrentContext()).FromJust();
    }
    // Default to string
    // Varsayilan olarak string'e donustur
    v8::String::Utf8Value str(iso, val);
    return std::string(*str ? *str : "");
}

// Helper: Convert OptionValue variant to nlohmann::json
// Yardimci: OptionValue variant'ini nlohmann::json'a donustur
static json OptionValueToJson(const OptionValue& ov) {
    return std::visit([](auto&& arg) -> json {
        return arg;
    }, ov);
}

// Context struct to pass buffer options pointer and i18n to lambda callbacks
// Lambda callback'lere hem buffer options hem i18n isaretcisini aktarmak icin baglam yapisi
struct OptionsCtx {
    BufferOptions* opts;
    I18n* i18n;
};

// Register editor.options JS object with all BufferOptions methods
// editor.options JS nesnesini tum BufferOptions metodlariyla kaydet
void RegisterBufferOptionsBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsOpts = v8::Object::New(isolate);

    auto* octx = new OptionsCtx{edCtx.bufferOptions, edCtx.i18n};

    // options.setDefault(key, value) -> {ok, data: true, ...} - Set global default option
    // Global varsayilan secenegi ayarla
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setDefault"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* oc = static_cast<OptionsCtx*>(args.Data().As<v8::External>()->Value());
            if (!oc || !oc->opts) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, oc ? oc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "key, value"}}, oc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            v8::String::Utf8Value key(iso, args[0]);
            OptionValue val = JsToOptionValue(iso, args[1]);
            oc->opts->setDefault(*key ? *key : "", val);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, octx)).ToLocalChecked()
    ).Check();

    // options.getDefault(key) -> {ok, data: value|null, ...} - Get global default option
    // Global varsayilan secenegi al
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getDefault"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* oc = static_cast<OptionsCtx*>(args.Data().As<v8::External>()->Value());
            if (!oc || !oc->opts) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, oc ? oc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "key"}}, oc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            v8::String::Utf8Value key(iso, args[0]);
            auto result = oc->opts->getDefault(*key ? *key : "");
            if (result.has_value()) {
                V8Response::ok(args, OptionValueToJson(*result));
            } else {
                V8Response::ok(args, nullptr);
            }
        }, v8::External::New(isolate, octx)).ToLocalChecked()
    ).Check();

    // options.setLocal(bufferId, key, value) -> {ok, data: true, ...} - Set buffer-local option
    // Buffer-yerel secenegi ayarla
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setLocal"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* oc = static_cast<OptionsCtx*>(args.Data().As<v8::External>()->Value());
            if (!oc || !oc->opts) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, oc ? oc->i18n : nullptr);
                return;
            }
            if (args.Length() < 3) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "bufferId, key, value"}}, oc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int bufferId = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            v8::String::Utf8Value key(iso, args[1]);
            OptionValue val = JsToOptionValue(iso, args[2]);
            oc->opts->setLocal(bufferId, *key ? *key : "", val);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, octx)).ToLocalChecked()
    ).Check();

    // options.removeLocal(bufferId, key) -> {ok, data: true, ...} - Remove buffer-local override
    // Buffer-yerel gecersiz kilmayi kaldir
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "removeLocal"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* oc = static_cast<OptionsCtx*>(args.Data().As<v8::External>()->Value());
            if (!oc || !oc->opts) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, oc ? oc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "bufferId, key"}}, oc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int bufferId = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            v8::String::Utf8Value key(iso, args[1]);
            oc->opts->removeLocal(bufferId, *key ? *key : "");
            V8Response::ok(args, true);
        }, v8::External::New(isolate, octx)).ToLocalChecked()
    ).Check();

    // options.get(bufferId, key) -> {ok, data: value|null, ...} - Get effective option (local > default)
    // Gecerli secenegi al (yerel > varsayilan)
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "get"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* oc = static_cast<OptionsCtx*>(args.Data().As<v8::External>()->Value());
            if (!oc || !oc->opts) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, oc ? oc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "bufferId, key"}}, oc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int bufferId = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            v8::String::Utf8Value key(iso, args[1]);
            auto result = oc->opts->get(bufferId, *key ? *key : "");
            if (result.has_value()) {
                V8Response::ok(args, OptionValueToJson(*result));
            } else {
                V8Response::ok(args, nullptr);
            }
        }, v8::External::New(isolate, octx)).ToLocalChecked()
    ).Check();

    // options.hasLocal(bufferId, key) -> {ok, data: bool, ...} - Check if buffer has local override
    // Buffer'in yerel gecersiz kilmasi olup olmadigini kontrol et
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "hasLocal"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* oc = static_cast<OptionsCtx*>(args.Data().As<v8::External>()->Value());
            if (!oc || !oc->opts) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, oc ? oc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "bufferId, key"}}, oc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int bufferId = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            v8::String::Utf8Value key(iso, args[1]);
            bool has = oc->opts->hasLocal(bufferId, *key ? *key : "");
            V8Response::ok(args, has);
        }, v8::External::New(isolate, octx)).ToLocalChecked()
    ).Check();

    // options.listKeys(bufferId) -> {ok, data: [keys...], meta: {total: N}} - List all option keys for buffer
    // Bir buffer icin tum secenek anahtarlarini listele
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listKeys"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* oc = static_cast<OptionsCtx*>(args.Data().As<v8::External>()->Value());
            if (!oc || !oc->opts) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, oc ? oc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "bufferId"}}, oc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int bufferId = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            auto keys = oc->opts->listKeys(bufferId);

            json arr = json::array();
            for (const auto& k : keys) {
                arr.push_back(k);
            }
            json meta = {{"total", keys.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, octx)).ToLocalChecked()
    ).Check();

    // options.listLocalKeys(bufferId) -> {ok, data: [keys...], meta: {total: N}} - List buffer-local override keys
    // Buffer-yerel gecersiz kilma anahtarlarini listele
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listLocalKeys"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* oc = static_cast<OptionsCtx*>(args.Data().As<v8::External>()->Value());
            if (!oc || !oc->opts) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, oc ? oc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "bufferId"}}, oc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int bufferId = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            auto keys = oc->opts->listLocalKeys(bufferId);

            json arr = json::array();
            for (const auto& k : keys) {
                arr.push_back(k);
            }
            json meta = {{"total", keys.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, octx)).ToLocalChecked()
    ).Check();

    // options.listDefaultKeys() -> {ok, data: [keys...], meta: {total: N}} - List all global default keys
    // Tum global varsayilan anahtarlari listele
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listDefaultKeys"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* oc = static_cast<OptionsCtx*>(args.Data().As<v8::External>()->Value());
            if (!oc || !oc->opts) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, oc ? oc->i18n : nullptr);
                return;
            }
            auto keys = oc->opts->listDefaultKeys();

            json arr = json::array();
            for (const auto& k : keys) {
                arr.push_back(k);
            }
            json meta = {{"total", keys.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, octx)).ToLocalChecked()
    ).Check();

    // options.clearBuffer(bufferId) -> {ok, data: true, ...} - Clear all local options for a buffer
    // Bir buffer icin tum yerel secenekleri temizle
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clearBuffer"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* oc = static_cast<OptionsCtx*>(args.Data().As<v8::External>()->Value());
            if (!oc || !oc->opts) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, oc ? oc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "bufferId"}}, oc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int bufferId = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            oc->opts->clearBuffer(bufferId);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, octx)).ToLocalChecked()
    ).Check();

    // options.getInt(bufferId, key, fallback) -> {ok, data: int, ...} - Get option as integer
    // Secenegi tam sayi olarak al
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getInt"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* oc = static_cast<OptionsCtx*>(args.Data().As<v8::External>()->Value());
            if (!oc || !oc->opts) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, oc ? oc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "bufferId, key"}}, oc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            int bufferId = args[0]->Int32Value(ctx).FromJust();
            v8::String::Utf8Value key(iso, args[1]);
            int fallback = 0;
            if (args.Length() >= 3) {
                fallback = args[2]->Int32Value(ctx).FromJust();
            }
            int result = oc->opts->getInt(bufferId, *key ? *key : "", fallback);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, octx)).ToLocalChecked()
    ).Check();

    // options.getBool(bufferId, key, fallback) -> {ok, data: bool, ...} - Get option as boolean
    // Secenegi mantiksal deger olarak al
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getBool"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* oc = static_cast<OptionsCtx*>(args.Data().As<v8::External>()->Value());
            if (!oc || !oc->opts) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, oc ? oc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "bufferId, key"}}, oc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            int bufferId = args[0]->Int32Value(ctx).FromJust();
            v8::String::Utf8Value key(iso, args[1]);
            bool fallback = false;
            if (args.Length() >= 3) {
                fallback = args[2]->BooleanValue(iso);
            }
            bool result = oc->opts->getBool(bufferId, *key ? *key : "", fallback);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, octx)).ToLocalChecked()
    ).Check();

    // options.getString(bufferId, key, fallback) -> {ok, data: "string", ...} - Get option as string
    // Secenegi metin olarak al
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getString"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* oc = static_cast<OptionsCtx*>(args.Data().As<v8::External>()->Value());
            if (!oc || !oc->opts) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, oc ? oc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "bufferId, key"}}, oc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            int bufferId = args[0]->Int32Value(ctx).FromJust();
            v8::String::Utf8Value key(iso, args[1]);
            std::string fallback = "";
            if (args.Length() >= 3) {
                v8::String::Utf8Value fb(iso, args[2]);
                fallback = *fb ? *fb : "";
            }
            std::string result = oc->opts->getString(bufferId, *key ? *key : "", fallback);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, octx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "options"),
        jsOpts).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _reg = [] {
    BindingRegistry::instance().registerBinding("options", RegisterBufferOptionsBinding);
    return true;
}();
