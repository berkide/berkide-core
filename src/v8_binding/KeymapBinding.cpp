// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "KeymapBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "KeymapManager.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Context struct for keymap binding lambdas
// Tus haritasi binding lambda'lari icin baglam yapisi
struct KeymapCtx {
    KeymapManager* mgr;
    I18n* i18n;
};

// Register editor.keymap JS object
// editor.keymap JS nesnesini kaydet
void RegisterKeymapBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsKm = v8::Object::New(isolate);

    auto* kctx = new KeymapCtx{edCtx.keymapManager, edCtx.i18n};

    // keymap.set(keymapName, keys, command, argsJson?) - Set a key binding
    // Tus baglantisi ayarla
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "set"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* kc = static_cast<KeymapCtx*>(args.Data().As<v8::External>()->Value());
            if (!kc || !kc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "keymapManager"}}, kc ? kc->i18n : nullptr);
                return;
            }
            if (args.Length() < 3) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "keymapName, keys, command"}}, kc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();

            std::string kmName  = v8Str(iso, args[0]);
            std::string keys    = v8Str(iso, args[1]);
            std::string command = v8Str(iso, args[2]);
            std::string argsJson = (args.Length() > 3) ? v8Str(iso, args[3]) : "";

            kc->mgr->set(kmName, keys, command, argsJson);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, kctx)).ToLocalChecked()
    ).Check();

    // keymap.remove(keymapName, keys) -> {ok, data: bool, ...}
    // Tus baglantisini kaldir
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "remove"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* kc = static_cast<KeymapCtx*>(args.Data().As<v8::External>()->Value());
            if (!kc || !kc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "keymapManager"}}, kc ? kc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "keymapName, keys"}}, kc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();

            std::string kmName = v8Str(iso, args[0]);
            std::string keys   = v8Str(iso, args[1]);
            bool removed = kc->mgr->remove(kmName, keys);
            V8Response::ok(args, removed);
        }, v8::External::New(isolate, kctx)).ToLocalChecked()
    ).Check();

    // keymap.lookup(keymapName, keys) -> {ok, data: {keys, command, argsJson} | null, ...}
    // Tus haritasi hiyerarsisinde ara
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "lookup"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* kc = static_cast<KeymapCtx*>(args.Data().As<v8::External>()->Value());
            if (!kc || !kc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "keymapManager"}}, kc ? kc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "keymapName, keys"}}, kc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();

            std::string kmName = v8Str(iso, args[0]);
            std::string keys   = v8Str(iso, args[1]);

            const KeyBinding* b = kc->mgr->lookup(kmName, keys);
            if (b) {
                json data = {
                    {"keys", b->keys},
                    {"command", b->command},
                    {"argsJson", b->argsJson}
                };
                V8Response::ok(args, data);
            } else {
                V8Response::ok(args, nullptr);
            }
        }, v8::External::New(isolate, kctx)).ToLocalChecked()
    ).Check();

    // keymap.feedKey(keymapName, key) -> {ok, data: command | "", ...}
    // Tus basisi besle
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "feedKey"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* kc = static_cast<KeymapCtx*>(args.Data().As<v8::External>()->Value());
            if (!kc || !kc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "keymapManager"}}, kc ? kc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "keymapName, key"}}, kc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();

            std::string kmName = v8Str(iso, args[0]);
            std::string key    = v8Str(iso, args[1]);

            std::string result = kc->mgr->feedKey(kmName, key);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, kctx)).ToLocalChecked()
    ).Check();

    // keymap.resetPrefix() - Cancel multi-key sequence
    // Coklu tus dizisini iptal et
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "resetPrefix"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* kc = static_cast<KeymapCtx*>(args.Data().As<v8::External>()->Value());
            if (!kc || !kc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "keymapManager"}}, kc ? kc->i18n : nullptr);
                return;
            }
            kc->mgr->resetPrefix();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, kctx)).ToLocalChecked()
    ).Check();

    // keymap.currentPrefix() -> {ok, data: string, ...}
    // Mevcut on ek tuslarini al
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "currentPrefix"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* kc = static_cast<KeymapCtx*>(args.Data().As<v8::External>()->Value());
            if (!kc || !kc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "keymapManager"}}, kc ? kc->i18n : nullptr);
                return;
            }
            const std::string& prefix = kc->mgr->currentPrefix();
            V8Response::ok(args, prefix);
        }, v8::External::New(isolate, kctx)).ToLocalChecked()
    ).Check();

    // keymap.hasPendingPrefix() -> {ok, data: bool, ...}
    // Bekleyen on ek olup olmadigini kontrol et
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "hasPendingPrefix"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* kc = static_cast<KeymapCtx*>(args.Data().As<v8::External>()->Value());
            if (!kc || !kc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "keymapManager"}}, kc ? kc->i18n : nullptr);
                return;
            }
            bool pending = kc->mgr->hasPendingPrefix();
            V8Response::ok(args, pending);
        }, v8::External::New(isolate, kctx)).ToLocalChecked()
    ).Check();

    // keymap.createKeymap(name, parent?) - Create a new keymap
    // Yeni tus haritasi olustur
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "createKeymap"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* kc = static_cast<KeymapCtx*>(args.Data().As<v8::External>()->Value());
            if (!kc || !kc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "keymapManager"}}, kc ? kc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name"}}, kc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();

            std::string name = v8Str(iso, args[0]);
            std::string parent = (args.Length() > 1) ? v8Str(iso, args[1]) : "";
            kc->mgr->createKeymap(name, parent);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, kctx)).ToLocalChecked()
    ).Check();

    // keymap.listBindings(keymapName) -> {ok, data: [{keys, command, argsJson}], meta: {total: N}, ...}
    // Tus haritasindaki tum baglantilari listele
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listBindings"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* kc = static_cast<KeymapCtx*>(args.Data().As<v8::External>()->Value());
            if (!kc || !kc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "keymapManager"}}, kc ? kc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "keymapName"}}, kc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();

            std::string kmName = v8Str(iso, args[0]);
            auto bindings = kc->mgr->listBindings(kmName);

            json arr = json::array();
            for (size_t i = 0; i < bindings.size(); ++i) {
                arr.push_back({
                    {"keys", bindings[i].keys},
                    {"command", bindings[i].command},
                    {"argsJson", bindings[i].argsJson}
                });
            }
            json meta = {{"total", bindings.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, kctx)).ToLocalChecked()
    ).Check();

    // keymap.listKeymaps() -> {ok, data: [string, ...], meta: {total: N}, ...}
    // Tum tus haritasi adlarini listele
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listKeymaps"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* kc = static_cast<KeymapCtx*>(args.Data().As<v8::External>()->Value());
            if (!kc || !kc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "keymapManager"}}, kc ? kc->i18n : nullptr);
                return;
            }

            auto names = kc->mgr->listKeymaps();
            json arr = json::array();
            for (size_t i = 0; i < names.size(); ++i) {
                arr.push_back(names[i]);
            }
            json meta = {{"total", names.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, kctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "keymap"),
        jsKm).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _keymapReg = [] {
    BindingRegistry::instance().registerBinding("keymap", RegisterKeymapBinding);
    return true;
}();
