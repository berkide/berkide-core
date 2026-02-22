// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "RegisterBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "RegisterManager.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Context struct for register binding lambdas
// Register binding lambda'lari icin baglam yapisi
struct RegisterCtx {
    RegisterManager* rm;
    I18n* i18n;
};

// Register editor.registers JS object with get, set, yank, paste, list, clear
// editor.registers JS nesnesini get, set, yank, paste, list, clear ile kaydet
void RegisterRegisterBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsRegs = v8::Object::New(isolate);

    auto* rctx = new RegisterCtx{ctx.registers, ctx.i18n};

    // registers.get(name) -> {ok, data: {content, linewise} | null, ...}
    // Register icerigini al
    jsRegs->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "get"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* rc = static_cast<RegisterCtx*>(args.Data().As<v8::External>()->Value());
            if (!rc || !rc->rm) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "registerManager"}}, rc ? rc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name"}}, rc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();

            std::string name = v8Str(iso, args[0]);
            RegisterEntry entry = rc->rm->get(name);

            if (entry.content.empty()) {
                V8Response::ok(args, nullptr);
                return;
            }

            json data = {
                {"content", entry.content},
                {"linewise", entry.linewise}
            };
            V8Response::ok(args, data);
        }, v8::External::New(isolate, rctx)).ToLocalChecked()
    ).Check();

    // registers.set(name, content, linewise?) - Set register content
    // Register icerigini ayarla
    jsRegs->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "set"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* rc = static_cast<RegisterCtx*>(args.Data().As<v8::External>()->Value());
            if (!rc || !rc->rm) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "registerManager"}}, rc ? rc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name, content"}}, rc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();

            std::string name = v8Str(iso, args[0]);
            std::string content = v8Str(iso, args[1]);
            bool linewise = (args.Length() > 2) ? args[2]->BooleanValue(iso) : false;
            rc->rm->set(name, content, linewise);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, rctx)).ToLocalChecked()
    ).Check();

    // registers.recordYank(content, linewise?) - Record a yank operation
    // Kopyalama islemini kaydet
    jsRegs->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "recordYank"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* rc = static_cast<RegisterCtx*>(args.Data().As<v8::External>()->Value());
            if (!rc || !rc->rm) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "registerManager"}}, rc ? rc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "content"}}, rc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();

            std::string content = v8Str(iso, args[0]);
            bool linewise = (args.Length() > 1) ? args[1]->BooleanValue(iso) : false;
            rc->rm->recordYank(content, linewise);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, rctx)).ToLocalChecked()
    ).Check();

    // registers.recordDelete(content, linewise?) - Record a delete operation
    // Silme islemini kaydet
    jsRegs->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "recordDelete"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* rc = static_cast<RegisterCtx*>(args.Data().As<v8::External>()->Value());
            if (!rc || !rc->rm) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "registerManager"}}, rc ? rc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "content"}}, rc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();

            std::string content = v8Str(iso, args[0]);
            bool linewise = (args.Length() > 1) ? args[1]->BooleanValue(iso) : false;
            rc->rm->recordDelete(content, linewise);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, rctx)).ToLocalChecked()
    ).Check();

    // registers.getUnnamed() -> {ok, data: {content, linewise} | null, ...}
    // Adsiz register'i al
    jsRegs->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getUnnamed"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* rc = static_cast<RegisterCtx*>(args.Data().As<v8::External>()->Value());
            if (!rc || !rc->rm) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "registerManager"}}, rc ? rc->i18n : nullptr);
                return;
            }

            RegisterEntry entry = rc->rm->getUnnamed();
            if (entry.content.empty()) {
                V8Response::ok(args, nullptr);
                return;
            }

            json data = {
                {"content", entry.content},
                {"linewise", entry.linewise}
            };
            V8Response::ok(args, data);
        }, v8::External::New(isolate, rctx)).ToLocalChecked()
    ).Check();

    // registers.list() -> {ok, data: [{name, content, linewise}], meta: {total: N}, ...}
    // Tum dolu register'lari listele
    jsRegs->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* rc = static_cast<RegisterCtx*>(args.Data().As<v8::External>()->Value());
            if (!rc || !rc->rm) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "registerManager"}}, rc ? rc->i18n : nullptr);
                return;
            }

            auto entries = rc->rm->list();
            json arr = json::array();
            for (size_t i = 0; i < entries.size(); ++i) {
                arr.push_back({
                    {"name", entries[i].first},
                    {"content", entries[i].second.content},
                    {"linewise", entries[i].second.linewise}
                });
            }
            json meta = {{"total", entries.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, rctx)).ToLocalChecked()
    ).Check();

    // registers.clear() - Clear all registers
    // Tum register'lari temizle
    jsRegs->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clear"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* rc = static_cast<RegisterCtx*>(args.Data().As<v8::External>()->Value());
            if (!rc || !rc->rm) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "registerManager"}}, rc ? rc->i18n : nullptr);
                return;
            }
            rc->rm->clearAll();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, rctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "registers"),
        jsRegs).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _registerReg = [] {
    BindingRegistry::instance().registerBinding("registers", RegisterRegisterBinding);
    return true;
}();
