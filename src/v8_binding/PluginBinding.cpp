// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "PluginBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "PluginManager.h"
#include <v8.h>

// Context struct for plugin binding lambdas
// Eklenti binding lambda'lari icin baglam yapisi
struct PluginCtx {
    PluginManager* pm;
    I18n* i18n;
};

// Register editor.plugins JS object with list(), enable(name), disable(name)
// editor.plugins JS nesnesini list(), enable(name), disable(name) ile kaydet
void RegisterPluginBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsPlugins = v8::Object::New(isolate);

    auto* pctx = new PluginCtx{ctx.pluginManager, ctx.i18n};

    // plugins.list() -> {ok, data: [{name, version, enabled, loaded, error?}], meta: {total: N}, ...}
    jsPlugins->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* pc = static_cast<PluginCtx*>(args.Data().As<v8::External>()->Value());
            if (!pc || !pc->pm) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "pluginManager"}}, pc ? pc->i18n : nullptr);
                return;
            }

            auto& list = pc->pm->list();
            json arr = json::array();
            for (size_t i = 0; i < list.size(); ++i) {
                auto& ps = list[i];
                json obj = {
                    {"name", ps.manifest.name},
                    {"version", ps.manifest.version},
                    {"enabled", ps.manifest.enabled},
                    {"loaded", ps.loaded}
                };
                if (ps.hasError) {
                    obj["error"] = ps.error;
                }
                arr.push_back(obj);
            }
            json meta = {{"total", list.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, pctx)).ToLocalChecked()
    ).Check();

    // plugins.enable(name) -> {ok, data: bool, ...}
    jsPlugins->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "enable"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* pc = static_cast<PluginCtx*>(args.Data().As<v8::External>()->Value());
            if (!pc || !pc->pm) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "pluginManager"}}, pc ? pc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name"}}, pc->i18n);
                return;
            }
            v8::String::Utf8Value name(args.GetIsolate(), args[0]);
            bool result = pc->pm->enable(*name ? *name : "");
            V8Response::ok(args, result);
        }, v8::External::New(isolate, pctx)).ToLocalChecked()
    ).Check();

    // plugins.disable(name) -> {ok, data: bool, ...}
    jsPlugins->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "disable"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* pc = static_cast<PluginCtx*>(args.Data().As<v8::External>()->Value());
            if (!pc || !pc->pm) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "pluginManager"}}, pc ? pc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name"}}, pc->i18n);
                return;
            }
            v8::String::Utf8Value name(args.GetIsolate(), args[0]);
            bool result = pc->pm->disable(*name ? *name : "");
            V8Response::ok(args, result);
        }, v8::External::New(isolate, pctx)).ToLocalChecked()
    ).Check();

    // plugins.discover(dir) - Discover plugins from a directory
    // Bir dizinden eklentileri kesfet
    jsPlugins->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "discover"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* pc = static_cast<PluginCtx*>(args.Data().As<v8::External>()->Value());
            if (!pc || !pc->pm) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "pluginManager"}}, pc ? pc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "dir"}}, pc->i18n);
                return;
            }
            v8::String::Utf8Value dir(args.GetIsolate(), args[0]);
            pc->pm->discover(*dir ? *dir : "");
            V8Response::ok(args, true);
        }, v8::External::New(isolate, pctx)).ToLocalChecked()
    ).Check();

    // plugins.activate(name) -> {ok, data: bool, ...} - Activate a plugin by name
    // Ismiyle bir eklentiyi etkinlestir
    jsPlugins->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "activate"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* pc = static_cast<PluginCtx*>(args.Data().As<v8::External>()->Value());
            if (!pc || !pc->pm) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "pluginManager"}}, pc ? pc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name"}}, pc->i18n);
                return;
            }
            v8::String::Utf8Value name(args.GetIsolate(), args[0]);
            bool result = pc->pm->activate(*name ? *name : "");
            V8Response::ok(args, result);
        }, v8::External::New(isolate, pctx)).ToLocalChecked()
    ).Check();

    // plugins.deactivate(name) -> {ok, data: bool, ...} - Deactivate a plugin by name
    // Ismiyle bir eklentiyi devre disi birak
    jsPlugins->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "deactivate"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* pc = static_cast<PluginCtx*>(args.Data().As<v8::External>()->Value());
            if (!pc || !pc->pm) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "pluginManager"}}, pc ? pc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name"}}, pc->i18n);
                return;
            }
            v8::String::Utf8Value name(args.GetIsolate(), args[0]);
            bool result = pc->pm->deactivate(*name ? *name : "");
            V8Response::ok(args, result);
        }, v8::External::New(isolate, pctx)).ToLocalChecked()
    ).Check();

    // plugins.find(name) -> {ok, data: {name, version, enabled, loaded, dirPath?, error?} | null, ...}
    // Ismiyle eklenti bul
    jsPlugins->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "find"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* pc = static_cast<PluginCtx*>(args.Data().As<v8::External>()->Value());
            if (!pc || !pc->pm) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "pluginManager"}}, pc ? pc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name"}}, pc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();

            v8::String::Utf8Value name(iso, args[0]);
            std::string nameStr = *name ? *name : "";
            PluginState* ps = pc->pm->find(nameStr);
            if (!ps) {
                V8Response::ok(args, nullptr);
                return;
            }

            json data = {
                {"name", ps->manifest.name},
                {"version", ps->manifest.version},
                {"enabled", ps->manifest.enabled},
                {"loaded", ps->loaded}
            };
            if (!ps->dirPath.empty()) {
                data["dirPath"] = ps->dirPath;
            }
            if (ps->hasError) {
                data["error"] = ps->error;
            }
            V8Response::ok(args, data);
        }, v8::External::New(isolate, pctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "plugins"),
        jsPlugins).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _pluginReg = [] {
    BindingRegistry::instance().registerBinding("plugins", RegisterPluginBinding);
    return true;
}();
