#include "PluginBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "PluginManager.h"
#include <v8.h>

// Register editor.plugins JS object with list(), enable(name), disable(name)
// editor.plugins JS nesnesini list(), enable(name), disable(name) ile kaydet
void RegisterPluginBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsPlugins = v8::Object::New(isolate);
    PluginManager* pm = ctx.pluginManager;

    // plugins.list() -> [{name, version, enabled, loaded, error}]
    jsPlugins->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* pm = static_cast<PluginManager*>(args.Data().As<v8::External>()->Value());
            if (!pm) return;
            auto* isolate = args.GetIsolate();
            auto ctx = isolate->GetCurrentContext();
            auto& list = pm->list();

            v8::Local<v8::Array> arr = v8::Array::New(isolate, static_cast<int>(list.size()));
            for (size_t i = 0; i < list.size(); ++i) {
                auto& ps = list[i];
                v8::Local<v8::Object> obj = v8::Object::New(isolate);
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "name"),
                    v8::String::NewFromUtf8(isolate, ps.manifest.name.c_str()).ToLocalChecked()).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "version"),
                    v8::String::NewFromUtf8(isolate, ps.manifest.version.c_str()).ToLocalChecked()).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "enabled"),
                    v8::Boolean::New(isolate, ps.manifest.enabled)).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "loaded"),
                    v8::Boolean::New(isolate, ps.loaded)).Check();
                if (ps.hasError) {
                    obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "error"),
                        v8::String::NewFromUtf8(isolate, ps.error.c_str()).ToLocalChecked()).Check();
                }
                arr->Set(ctx, static_cast<uint32_t>(i), obj).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, pm)).ToLocalChecked()
    ).Check();

    // plugins.enable(name)
    jsPlugins->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "enable"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            auto* pm = static_cast<PluginManager*>(args.Data().As<v8::External>()->Value());
            if (!pm) return;
            v8::String::Utf8Value name(args.GetIsolate(), args[0]);
            bool ok = pm->enable(*name ? *name : "");
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, pm)).ToLocalChecked()
    ).Check();

    // plugins.disable(name)
    jsPlugins->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "disable"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            auto* pm = static_cast<PluginManager*>(args.Data().As<v8::External>()->Value());
            if (!pm) return;
            v8::String::Utf8Value name(args.GetIsolate(), args[0]);
            bool ok = pm->disable(*name ? *name : "");
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, pm)).ToLocalChecked()
    ).Check();

    // plugins.discover(dir) - Discover plugins from a directory
    // Bir dizinden eklentileri kesfet
    jsPlugins->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "discover"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* pm = static_cast<PluginManager*>(args.Data().As<v8::External>()->Value());
            if (!pm || args.Length() < 1) return;
            v8::String::Utf8Value dir(args.GetIsolate(), args[0]);
            pm->discover(*dir ? *dir : "");
        }, v8::External::New(isolate, pm)).ToLocalChecked()
    ).Check();

    // plugins.activate(name) -> bool - Activate a plugin by name
    // Ismiyle bir eklentiyi etkinlestir
    jsPlugins->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "activate"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* pm = static_cast<PluginManager*>(args.Data().As<v8::External>()->Value());
            if (!pm || args.Length() < 1) return;
            v8::String::Utf8Value name(args.GetIsolate(), args[0]);
            bool ok = pm->activate(*name ? *name : "");
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, pm)).ToLocalChecked()
    ).Check();

    // plugins.deactivate(name) -> bool - Deactivate a plugin by name
    // Ismiyle bir eklentiyi devre disi birak
    jsPlugins->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "deactivate"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* pm = static_cast<PluginManager*>(args.Data().As<v8::External>()->Value());
            if (!pm || args.Length() < 1) return;
            v8::String::Utf8Value name(args.GetIsolate(), args[0]);
            bool ok = pm->deactivate(*name ? *name : "");
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, pm)).ToLocalChecked()
    ).Check();

    // plugins.find(name) -> {name, version, enabled, loaded, error} | null
    // Ismiyle eklenti bul
    jsPlugins->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "find"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* pm = static_cast<PluginManager*>(args.Data().As<v8::External>()->Value());
            if (!pm || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            v8::String::Utf8Value name(iso, args[0]);
            std::string nameStr = *name ? *name : "";
            PluginState* ps = pm->find(nameStr);
            if (!ps) {
                args.GetReturnValue().SetNull();
                return;
            }

            v8::Local<v8::Object> obj = v8::Object::New(iso);
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "name"),
                v8::String::NewFromUtf8(iso, ps->manifest.name.c_str()).ToLocalChecked()).Check();
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "version"),
                v8::String::NewFromUtf8(iso, ps->manifest.version.c_str()).ToLocalChecked()).Check();
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "enabled"),
                v8::Boolean::New(iso, ps->manifest.enabled)).Check();
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "loaded"),
                v8::Boolean::New(iso, ps->loaded)).Check();
            if (!ps->dirPath.empty()) {
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "dirPath"),
                    v8::String::NewFromUtf8(iso, ps->dirPath.c_str()).ToLocalChecked()).Check();
            }
            if (ps->hasError) {
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "error"),
                    v8::String::NewFromUtf8(iso, ps->error.c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(obj);
        }, v8::External::New(isolate, pm)).ToLocalChecked()
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
