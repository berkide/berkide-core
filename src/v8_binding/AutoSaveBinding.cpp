// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "AutoSaveBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "AutoSave.h"
#include "buffers.h"
#include "state.h"
#include <v8.h>

// Context for autosave binding with i18n support
// i18n destekli otomatik kaydetme binding baglami
struct AutoSaveBindCtx {
    AutoSave* autoSave;
    Buffers* bufs;
    I18n* i18n;
};

// Register editor.autosave JS object with all public methods
// editor.autosave JS nesnesini tum genel metodlarla kaydet
void RegisterAutoSaveBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsAS = v8::Object::New(isolate);

    auto* actx = new AutoSaveBindCtx{edCtx.autoSave, edCtx.buffers, edCtx.i18n};

    // autosave.start() - Start auto-save background thread
    // Otomatik kaydetme arka plan thread'ini baslat
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "start"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, a ? a->i18n : nullptr);
                return;
            }
            a->autoSave->start();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.stop() - Stop auto-save background thread
    // Otomatik kaydetme arka plan thread'ini durdur
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "stop"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, a ? a->i18n : nullptr);
                return;
            }
            a->autoSave->stop();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.setInterval(seconds) - Set auto-save interval
    // Otomatik kaydetme araligini ayarla
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setInterval"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, a ? a->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "seconds"}}, a->i18n);
                return;
            }
            int sec = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromJust();
            a->autoSave->setInterval(sec);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.setDirectory(path) - Set auto-save directory
    // Otomatik kaydetme dizinini ayarla
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setDirectory"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, a ? a->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "path"}}, a->i18n);
                return;
            }
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            a->autoSave->setDirectory(*path ? *path : "");
            V8Response::ok(args, true);
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.createBackup(filePath) -> {ok, data: bool, ...} - Create backup before first write
    // Ilk yazmadan once yedek olustur
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "createBackup"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, a ? a->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "filePath"}}, a->i18n);
                return;
            }
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            bool ok = a->autoSave->createBackup(*path ? *path : "");
            V8Response::ok(args, ok);
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.saveBuffer(filePath, content) -> {ok, data: bool, ...} - Save buffer to recovery
    // Buffer'i kurtarma dosyasina kaydet
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "saveBuffer"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, a ? a->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "filePath, content"}}, a->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            v8::String::Utf8Value path(iso, args[0]);
            v8::String::Utf8Value content(iso, args[1]);
            bool ok = a->autoSave->saveBuffer(*path ? *path : "", *content ? *content : "");
            V8Response::ok(args, ok);
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.removeRecovery(filePath) -> {ok, data: true, ...} - Remove recovery file
    // Kurtarma dosyasini kaldir
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "removeRecovery"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, a ? a->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "filePath"}}, a->i18n);
                return;
            }
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            a->autoSave->removeRecovery(*path ? *path : "");
            V8Response::ok(args, true);
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.listRecoveryFiles() -> {ok, data: [{originalPath, recoveryPath, timestamp}, ...], ...}
    // Kurtarma dosyalarini listele
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listRecoveryFiles"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, a ? a->i18n : nullptr);
                return;
            }

            auto files = a->autoSave->listRecoveryFiles();
            json arr = json::array();
            for (size_t i = 0; i < files.size(); ++i) {
                arr.push_back(json({
                    {"originalPath", files[i].originalPath},
                    {"recoveryPath", files[i].recoveryPath},
                    {"timestamp", files[i].timestamp}
                }));
            }
            json meta = {{"total", files.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.hasExternalChange(filePath) -> {ok, data: bool, ...} - Check if file changed externally
    // Dosyanin harici olarak degistirilip degistirilmedigini kontrol et
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "hasExternalChange"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, a ? a->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "filePath"}}, a->i18n);
                return;
            }
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            bool changed = a->autoSave->hasExternalChange(*path ? *path : "");
            V8Response::ok(args, changed);
        }, v8::External::New(isolate, actx)).ToLocalChecked()
    ).Check();

    // autosave.recordMtime(filePath) -> {ok, data: true, ...} - Record file modification time
    // Dosya degistirilme zamanini kaydet
    jsAS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "recordMtime"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* a = static_cast<AutoSaveBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!a || !a->autoSave) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, a ? a->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "filePath"}}, a->i18n);
                return;
            }
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            a->autoSave->recordMtime(*path ? *path : "");
            V8Response::ok(args, true);
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
