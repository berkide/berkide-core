// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "I18nBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "I18n.h"
#include "V8ResponseBuilder.h"
#include <v8.h>

// Register editor.i18n JS object with translation and locale management methods
// Ceviri ve yerel ayar yonetim metodlariyla editor.i18n JS nesnesini kaydet
void RegisterI18nBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsI18n = v8::Object::New(isolate);

    I18n* i18n = ctx.i18n;

    // i18n.t(key, params?) -> string: translate a key with optional parameters
    // Istege bagli parametrelerle bir anahtari cevir
    jsI18n->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "t"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* i18n = static_cast<I18n*>(args.Data().As<v8::External>()->Value());
            if (!i18n || args.Length() < 1) {
                args.GetReturnValue().Set(v8::String::NewFromUtf8Literal(args.GetIsolate(), ""));
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            v8::String::Utf8Value keyStr(iso, args[0]);
            std::string key = *keyStr ? *keyStr : "";

            // Extract params from optional second argument
            // Istege bagli ikinci argumandan parametreleri cikar
            std::unordered_map<std::string, std::string> params;
            if (args.Length() > 1 && args[1]->IsObject()) {
                auto obj = args[1].As<v8::Object>();
                auto names = obj->GetOwnPropertyNames(ctx).ToLocalChecked();
                for (uint32_t i = 0; i < names->Length(); ++i) {
                    auto k = names->Get(ctx, i).ToLocalChecked();
                    auto v = obj->Get(ctx, k).ToLocalChecked();
                    v8::String::Utf8Value kStr(iso, k);
                    v8::String::Utf8Value vStr(iso, v);
                    if (*kStr && *vStr) {
                        params[*kStr] = *vStr;
                    }
                }
            }

            std::string result = i18n->t(key, params);
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(iso, result.c_str()).ToLocalChecked());
        }, v8::External::New(isolate, i18n)).ToLocalChecked()
    ).Check();

    // i18n.setLocale(locale) -> void: set the active locale
    // Aktif yerel ayari belirle
    jsI18n->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setLocale"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* i18n = static_cast<I18n*>(args.Data().As<v8::External>()->Value());
            if (!i18n || args.Length() < 1) return;
            v8::String::Utf8Value locale(args.GetIsolate(), args[0]);
            if (*locale) i18n->setLocale(*locale);
        }, v8::External::New(isolate, i18n)).ToLocalChecked()
    ).Check();

    // i18n.locale() -> string: get the current locale
    // Mevcut yerel ayari al
    jsI18n->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "locale"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* i18n = static_cast<I18n*>(args.Data().As<v8::External>()->Value());
            if (!i18n) return;
            auto* iso = args.GetIsolate();
            std::string loc = i18n->locale();
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(iso, loc.c_str()).ToLocalChecked());
        }, v8::External::New(isolate, i18n)).ToLocalChecked()
    ).Check();

    // i18n.register(locale, keys) -> void: register translation keys at runtime
    // Calisma zamaninda ceviri anahtarlarini kaydet
    jsI18n->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "register"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* i18n = static_cast<I18n*>(args.Data().As<v8::External>()->Value());
            if (!i18n || args.Length() < 2 || !args[1]->IsObject()) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            v8::String::Utf8Value locale(iso, args[0]);
            if (!*locale) return;

            // Convert V8 object to nlohmann::json
            // V8 nesnesini nlohmann::json'a donustur
            auto obj = args[1].As<v8::Object>();
            auto names = obj->GetOwnPropertyNames(ctx).ToLocalChecked();
            json keys = json::object();
            for (uint32_t i = 0; i < names->Length(); ++i) {
                auto k = names->Get(ctx, i).ToLocalChecked();
                auto v = obj->Get(ctx, k).ToLocalChecked();
                v8::String::Utf8Value kStr(iso, k);
                v8::String::Utf8Value vStr(iso, v);
                if (*kStr && *vStr) {
                    keys[*kStr] = std::string(*vStr);
                }
            }
            i18n->registerKeys(*locale, keys);
        }, v8::External::New(isolate, i18n)).ToLocalChecked()
    ).Check();

    // i18n.has(key) -> bool: check if a translation key exists
    // Bir ceviri anahtarinin var olup olmadigini kontrol et
    jsI18n->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "has"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* i18n = static_cast<I18n*>(args.Data().As<v8::External>()->Value());
            if (!i18n || args.Length() < 1) {
                args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), false));
                return;
            }
            v8::String::Utf8Value key(args.GetIsolate(), args[0]);
            args.GetReturnValue().Set(
                v8::Boolean::New(args.GetIsolate(), *key ? i18n->has(*key) : false));
        }, v8::External::New(isolate, i18n)).ToLocalChecked()
    ).Check();

    // i18n.locales() -> string[]: list all loaded locales
    // Tum yuklenmis yerel ayarlari listele
    jsI18n->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "locales"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* i18n = static_cast<I18n*>(args.Data().As<v8::External>()->Value());
            if (!i18n) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            auto locs = i18n->locales();
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(locs.size()));
            for (size_t i = 0; i < locs.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i),
                    v8::String::NewFromUtf8(iso, locs[i].c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, i18n)).ToLocalChecked()
    ).Check();

    // i18n.keys(locale?) -> string[]: list all keys for a locale
    // Bir yerel ayar icin tum anahtarlari listele
    jsI18n->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "keys"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* i18n = static_cast<I18n*>(args.Data().As<v8::External>()->Value());
            if (!i18n) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string locale = i18n->locale();
            if (args.Length() > 0) {
                v8::String::Utf8Value loc(iso, args[0]);
                if (*loc) locale = *loc;
            }

            auto ks = i18n->keys(locale);
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(ks.size()));
            for (size_t i = 0; i < ks.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i),
                    v8::String::NewFromUtf8(iso, ks[i].c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, i18n)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx, v8::String::NewFromUtf8Literal(isolate, "i18n"), jsI18n).Check();
}

// Auto-register "i18n" binding at static init time
// "i18n" binding'ini statik baslangicta otomatik kaydet
static bool _i18nReg = [] {
    BindingRegistry::instance().registerBinding("i18n",
        [](v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
            RegisterI18nBinding(isolate, editorObj, ctx);
        });
    return true;
}();
