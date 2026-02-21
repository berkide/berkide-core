#include "KeymapBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "KeymapManager.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Register editor.keymap JS object
// editor.keymap JS nesnesini kaydet
void RegisterKeymapBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsKm = v8::Object::New(isolate);

    auto* mgr = edCtx.keymapManager;

    // keymap.set(keymapName, keys, command, argsJson?) - Set a key binding
    // Tus baglantisi ayarla
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "set"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<KeymapManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 3) return;
            auto* iso = args.GetIsolate();

            std::string kmName  = v8Str(iso, args[0]);
            std::string keys    = v8Str(iso, args[1]);
            std::string command = v8Str(iso, args[2]);
            std::string argsJson = (args.Length() > 3) ? v8Str(iso, args[3]) : "";

            m->set(kmName, keys, command, argsJson);
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // keymap.remove(keymapName, keys) -> bool
    // Tus baglantisini kaldir
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "remove"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<KeymapManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 2) return;
            auto* iso = args.GetIsolate();

            std::string kmName = v8Str(iso, args[0]);
            std::string keys   = v8Str(iso, args[1]);
            args.GetReturnValue().Set(v8::Boolean::New(iso, m->remove(kmName, keys)));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // keymap.lookup(keymapName, keys) -> {keys, command, argsJson} | null
    // Tus haritasi hiyerarsisinde ara
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "lookup"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<KeymapManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string kmName = v8Str(iso, args[0]);
            std::string keys   = v8Str(iso, args[1]);

            const KeyBinding* b = m->lookup(kmName, keys);
            if (b) {
                v8::Local<v8::Object> obj = v8::Object::New(iso);
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "keys"),
                    v8::String::NewFromUtf8(iso, b->keys.c_str()).ToLocalChecked()).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "command"),
                    v8::String::NewFromUtf8(iso, b->command.c_str()).ToLocalChecked()).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "argsJson"),
                    v8::String::NewFromUtf8(iso, b->argsJson.c_str()).ToLocalChecked()).Check();
                args.GetReturnValue().Set(obj);
            } else {
                args.GetReturnValue().SetNull();
            }
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // keymap.feedKey(keymapName, key) -> command | "" | "UNBOUND"
    // Tus basisi besle
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "feedKey"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<KeymapManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 2) return;
            auto* iso = args.GetIsolate();

            std::string kmName = v8Str(iso, args[0]);
            std::string key    = v8Str(iso, args[1]);

            std::string result = m->feedKey(kmName, key);
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(iso, result.c_str()).ToLocalChecked());
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // keymap.resetPrefix() - Cancel multi-key sequence
    // Coklu tus dizisini iptal et
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "resetPrefix"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<KeymapManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            m->resetPrefix();
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // keymap.currentPrefix() -> string
    // Mevcut on ek tuslarini al
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "currentPrefix"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<KeymapManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            auto* iso = args.GetIsolate();
            const std::string& prefix = m->currentPrefix();
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(iso, prefix.c_str()).ToLocalChecked());
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // keymap.hasPendingPrefix() -> bool
    // Bekleyen on ek olup olmadigini kontrol et
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "hasPendingPrefix"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<KeymapManager*>(args.Data().As<v8::External>()->Value());
            if (!m) { args.GetReturnValue().Set(false); return; }
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), m->hasPendingPrefix()));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // keymap.createKeymap(name, parent?) - Create a new keymap
    // Yeni tus haritasi olustur
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "createKeymap"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<KeymapManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 1) return;
            auto* iso = args.GetIsolate();

            std::string name = v8Str(iso, args[0]);
            std::string parent = (args.Length() > 1) ? v8Str(iso, args[1]) : "";
            m->createKeymap(name, parent);
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // keymap.listBindings(keymapName) -> [{keys, command, argsJson}, ...]
    // Tus haritasindaki tum baglantilari listele
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listBindings"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<KeymapManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string kmName = v8Str(iso, args[0]);
            auto bindings = m->listBindings(kmName);

            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(bindings.size()));
            for (size_t i = 0; i < bindings.size(); ++i) {
                v8::Local<v8::Object> obj = v8::Object::New(iso);
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "keys"),
                    v8::String::NewFromUtf8(iso, bindings[i].keys.c_str()).ToLocalChecked()).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "command"),
                    v8::String::NewFromUtf8(iso, bindings[i].command.c_str()).ToLocalChecked()).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "argsJson"),
                    v8::String::NewFromUtf8(iso, bindings[i].argsJson.c_str()).ToLocalChecked()).Check();
                arr->Set(ctx, static_cast<uint32_t>(i), obj).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // keymap.listKeymaps() -> [string, ...]
    // Tum tus haritasi adlarini listele
    jsKm->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listKeymaps"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<KeymapManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            auto names = m->listKeymaps();
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(names.size()));
            for (size_t i = 0; i < names.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i),
                    v8::String::NewFromUtf8(iso, names[i].c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
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
