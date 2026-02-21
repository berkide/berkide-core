#include "RegisterBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "RegisterManager.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Register editor.registers JS object with get, set, yank, paste, list, clear
// editor.registers JS nesnesini get, set, yank, paste, list, clear ile kaydet
void RegisterRegisterBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsRegs = v8::Object::New(isolate);
    RegisterManager* rm = ctx.registers;

    // registers.get(name) -> {content, linewise} | null
    // Register icerigini al
    jsRegs->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "get"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* rm = static_cast<RegisterManager*>(args.Data().As<v8::External>()->Value());
            if (!rm || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string name = v8Str(iso, args[0]);
            RegisterEntry entry = rm->get(name);

            if (entry.content.empty()) {
                args.GetReturnValue().SetNull();
                return;
            }

            v8::Local<v8::Object> obj = v8::Object::New(iso);
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "content"),
                v8::String::NewFromUtf8(iso, entry.content.c_str()).ToLocalChecked()).Check();
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "linewise"),
                v8::Boolean::New(iso, entry.linewise)).Check();
            args.GetReturnValue().Set(obj);
        }, v8::External::New(isolate, rm)).ToLocalChecked()
    ).Check();

    // registers.set(name, content, linewise?) - Set register content
    // Register icerigini ayarla
    jsRegs->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "set"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* rm = static_cast<RegisterManager*>(args.Data().As<v8::External>()->Value());
            if (!rm || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string name = v8Str(iso, args[0]);
            std::string content = v8Str(iso, args[1]);
            bool linewise = (args.Length() > 2) ? args[2]->BooleanValue(iso) : false;
            rm->set(name, content, linewise);
        }, v8::External::New(isolate, rm)).ToLocalChecked()
    ).Check();

    // registers.recordYank(content, linewise?) - Record a yank operation
    // Kopyalama islemini kaydet
    jsRegs->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "recordYank"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* rm = static_cast<RegisterManager*>(args.Data().As<v8::External>()->Value());
            if (!rm || args.Length() < 1) return;
            auto* iso = args.GetIsolate();

            std::string content = v8Str(iso, args[0]);
            bool linewise = (args.Length() > 1) ? args[1]->BooleanValue(iso) : false;
            rm->recordYank(content, linewise);
        }, v8::External::New(isolate, rm)).ToLocalChecked()
    ).Check();

    // registers.recordDelete(content, linewise?) - Record a delete operation
    // Silme islemini kaydet
    jsRegs->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "recordDelete"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* rm = static_cast<RegisterManager*>(args.Data().As<v8::External>()->Value());
            if (!rm || args.Length() < 1) return;
            auto* iso = args.GetIsolate();

            std::string content = v8Str(iso, args[0]);
            bool linewise = (args.Length() > 1) ? args[1]->BooleanValue(iso) : false;
            rm->recordDelete(content, linewise);
        }, v8::External::New(isolate, rm)).ToLocalChecked()
    ).Check();

    // registers.getUnnamed() -> {content, linewise} | null
    // Adsiz register'i al
    jsRegs->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getUnnamed"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* rm = static_cast<RegisterManager*>(args.Data().As<v8::External>()->Value());
            if (!rm) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            RegisterEntry entry = rm->getUnnamed();
            if (entry.content.empty()) {
                args.GetReturnValue().SetNull();
                return;
            }

            v8::Local<v8::Object> obj = v8::Object::New(iso);
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "content"),
                v8::String::NewFromUtf8(iso, entry.content.c_str()).ToLocalChecked()).Check();
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "linewise"),
                v8::Boolean::New(iso, entry.linewise)).Check();
            args.GetReturnValue().Set(obj);
        }, v8::External::New(isolate, rm)).ToLocalChecked()
    ).Check();

    // registers.list() -> [{name, content, linewise}]
    // Tum dolu register'lari listele
    jsRegs->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* rm = static_cast<RegisterManager*>(args.Data().As<v8::External>()->Value());
            if (!rm) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            auto entries = rm->list();
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(entries.size()));
            for (size_t i = 0; i < entries.size(); ++i) {
                v8::Local<v8::Object> obj = v8::Object::New(iso);
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "name"),
                    v8::String::NewFromUtf8(iso, entries[i].first.c_str()).ToLocalChecked()).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "content"),
                    v8::String::NewFromUtf8(iso, entries[i].second.content.c_str()).ToLocalChecked()).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "linewise"),
                    v8::Boolean::New(iso, entries[i].second.linewise)).Check();
                arr->Set(ctx, static_cast<uint32_t>(i), obj).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, rm)).ToLocalChecked()
    ).Check();

    // registers.clear() - Clear all registers
    // Tum register'lari temizle
    jsRegs->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clear"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* rm = static_cast<RegisterManager*>(args.Data().As<v8::External>()->Value());
            if (!rm) return;
            rm->clearAll();
        }, v8::External::New(isolate, rm)).ToLocalChecked()
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
