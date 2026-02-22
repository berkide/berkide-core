// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "WorkerBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "WorkerManager.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Context for worker binding
// Worker binding baglami
struct WorkerCtx {
    EditorContext* edCtx;
    I18n* i18n;
};

// Register editor.workers JS binding with standard response format
// Standart yanit formatiyla editor.workers JS binding'ini kaydet
void RegisterWorkerBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto context = isolate->GetCurrentContext();
    auto workersObj = v8::Object::New(isolate);

    auto* wctx = new WorkerCtx{&ctx, ctx.i18n};

    // editor.workers.create(scriptPath) -> {ok, data: workerId}
    // Betik yolundan worker olustur
    workersObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "create"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WorkerCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->edCtx || !wc->edCtx->workerManager) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "workerManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "scriptPath"}}, wc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            std::string path = v8Str(iso, args[0]);
            int id = wc->edCtx->workerManager->createWorker(path);
            V8Response::ok(args, id);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // editor.workers.createFromSource(source) -> {ok, data: workerId}
    // Kaynak koddan worker olustur
    workersObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "createFromSource"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WorkerCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->edCtx || !wc->edCtx->workerManager) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "workerManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "source"}}, wc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            std::string source = v8Str(iso, args[0]);
            int id = wc->edCtx->workerManager->createWorkerFromSource(source);
            V8Response::ok(args, id);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // editor.workers.postMessage(workerId, message) -> {ok, data: bool}
    // Worker'a mesaj gonder
    workersObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "postMessage"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WorkerCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->edCtx || !wc->edCtx->workerManager) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "workerManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "workerId, message"}}, wc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            int workerId = args[0]->Int32Value(ctx2).FromMaybe(-1);
            std::string message = v8Str(iso, args[1]);
            bool ok = wc->edCtx->workerManager->postMessage(workerId, message);
            V8Response::ok(args, ok);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // editor.workers.terminate(workerId) -> {ok, data: bool}
    // Worker'i sonlandir
    workersObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "terminate"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WorkerCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->edCtx || !wc->edCtx->workerManager) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "workerManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "workerId"}}, wc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            int workerId = args[0]->Int32Value(ctx2).FromMaybe(-1);
            bool ok = wc->edCtx->workerManager->terminate(workerId);
            V8Response::ok(args, ok);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // editor.workers.terminateAll() -> {ok, data: true}
    // Tum worker'lari sonlandir
    workersObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "terminateAll"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WorkerCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->edCtx || !wc->edCtx->workerManager) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "workerManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            wc->edCtx->workerManager->terminateAll();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // editor.workers.state(workerId) -> {ok, data: "pending"|"running"|"stopped"|"error"}
    // Worker durumunu al
    workersObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "state"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WorkerCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->edCtx || !wc->edCtx->workerManager) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "workerManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "workerId"}}, wc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            int workerId = args[0]->Int32Value(ctx2).FromMaybe(-1);
            WorkerState st = wc->edCtx->workerManager->getState(workerId);
            std::string result = "stopped";
            switch (st) {
                case WorkerState::Pending: result = "pending"; break;
                case WorkerState::Running: result = "running"; break;
                case WorkerState::Error:   result = "error"; break;
                default: break;
            }
            V8Response::ok(args, result);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // editor.workers.activeCount() -> {ok, data: number}
    // Aktif worker sayisini al
    workersObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "activeCount"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WorkerCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->edCtx || !wc->edCtx->workerManager) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "workerManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            V8Response::ok(args, wc->edCtx->workerManager->activeCount());
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // editor.workers.onMessage(callback) -> {ok, data: true}
    // Worker'lardan gelen mesajlar icin geri cagirim kaydet
    workersObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "onMessage"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WorkerCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->edCtx || !wc->edCtx->workerManager) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "workerManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1 || !args[0]->IsFunction()) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "callback"}}, wc->i18n);
                return;
            }
            auto iso = args.GetIsolate();

            // Store the JS callback as a persistent handle
            // JS geri cagirimini kalici tutamak olarak sakla
            auto* persistent = new v8::Global<v8::Function>(iso, args[0].As<v8::Function>());
            auto* isoPtr = iso;

            wc->edCtx->workerManager->setMessageCallback(
                [persistent, isoPtr](int workerId, const std::string& message) {
                    // This callback is invoked from processPendingMessages on the main thread
                    // Bu geri cagirim ana thread'de processPendingMessages'dan cagrilir
                    v8::HandleScope scope(isoPtr);
                    auto context = isoPtr->GetCurrentContext();
                    if (context.IsEmpty()) return;

                    v8::Local<v8::Function> fn = persistent->Get(isoPtr);
                    v8::Local<v8::Value> argv[2] = {
                        v8::Integer::New(isoPtr, workerId),
                        v8::String::NewFromUtf8(isoPtr, message.c_str()).ToLocalChecked()
                    };
                    fn->Call(context, v8::Undefined(isoPtr), 2, argv).FromMaybe(v8::Local<v8::Value>());
                }
            );
            V8Response::ok(args, true);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "workers"),
        workersObj).Check();
}

// Self-register at static initialization time
// Statik baslatma zamaninda kendini kaydet
static bool _workerReg = [] {
    BindingRegistry::instance().registerBinding("workers", RegisterWorkerBinding);
    return true;
}();
