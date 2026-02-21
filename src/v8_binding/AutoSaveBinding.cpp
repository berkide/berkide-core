#include "AutoSaveBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "AutoSave.h"
#include "buffers.h"
#include "state.h"
#include <v8.h>

// Context for autosave binding
// Otomatik kaydetme binding baglami
struct AutoSaveBindCtx {
    AutoSave* autoSave;
    Buffers* bufs;
};

// Register editor.autosave JS object with all public methods
// editor.autosave JS nesnesini tum genel metodlarla kaydet
void RegisterAutoSaveBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsAS = v8::Object::New(isolate);

    auto* actx = new AutoSaveBindCtx{edCtx.autoSave, edCtx.buffers};

    // autosave.start() - Start auto-save background thread
    // Otomatik kaydetme arka plan thread'ini baslat
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "start"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave) return;
            a->autoSave->start();
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.stop() - Stop auto-save background thread
    // Otomatik kaydetme arka plan thread'ini durdur
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "stop"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave) return;
            a->autoSave->stop();
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.setInterval(seconds) - Set auto-save interval
    // Otomatik kaydetme araligini ayarla
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setInterval"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave || args.Length() < 1) return;
            int sec = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromJust();
            a->autoSave->setInterval(sec);
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.setDirectory(path) - Set auto-save directory
    // Otomatik kaydetme dizinini ayarla
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setDirectory"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave || args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            a->autoSave->setDirectory(*path ? *path : "");
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.createBackup(filePath) -> bool - Create backup before first write
    // Ilk yazmadan once yedek olustur
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "createBackup"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave || args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            bool ok = a->autoSave->createBackup(*path ? *path : "");
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.saveBuffer(filePath, content) -> bool - Save buffer to recovery
    // Buffer'i kurtarma dosyasina kaydet
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "saveBuffer"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            v8::String::Utf8Value path(iso, args[0]);
            v8::String::Utf8Value content(iso, args[1]);
            bool ok = a->autoSave->saveBuffer(*path ? *path : "", *content ? *content : "");
            args.GetReturnValue().Set(v8::Boolean::New(iso, ok));
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.removeRecovery(filePath) - Remove recovery file
    // Kurtarma dosyasini kaldir
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "removeRecovery"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave || args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            a->autoSave->removeRecovery(*path ? *path : "");
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.listRecoveryFiles() -> [{originalPath, recoveryPath, timestamp}]
    // Kurtarma dosyalarini listele
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listRecoveryFiles"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            auto files = a->autoSave->listRecoveryFiles();
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(files.size()));
            for (size_t i = 0; i < files.size(); ++i) {
                v8::Local<v8::Object> obj = v8::Object::New(iso);
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "originalPath"),
                    v8::String::NewFromUtf8(iso, files[i].originalPath.c_str()).ToLocalChecked()).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "recoveryPath"),
                    v8::String::NewFromUtf8(iso, files[i].recoveryPath.c_str()).ToLocalChecked()).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "timestamp"),
                    v8::String::NewFromUtf8(iso, files[i].timestamp.c_str()).ToLocalChecked()).Check();
                arr->Set(ctx, static_cast<uint32_t>(i), obj).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.hasExternalChange(filePath) -> bool - Check if file changed externally
    // Dosyanin harici olarak degistirilip degistirilmedigini kontrol et
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "hasExternalChange"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave || args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            bool changed = a->autoSave->hasExternalChange(*path ? *path : "");
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), changed));
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.recordMtime(filePath) - Record file modification time
    // Dosya degistirilme zamanini kaydet
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "recordMtime"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave || args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            a->autoSave->recordMtime(*path ? *path : "");
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "autosave"),
        jsAS).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _autoSaveReg = [] {
    BindingRegistry::instance().registerBinding("autosave", RegisterAutoSaveBinding);
    return true;
}();
