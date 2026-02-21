#include "WorkerBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "WorkerManager.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Register editor.workers JS binding
// editor.workers JS binding'ini kaydet
void RegisterWorkerBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto context = isolate->GetCurrentContext();
    auto workersObj = v8::Object::New(isolate);

    // editor.workers.create(scriptPath) -> workerId
    // editor.workers.olustur(betikYolu) -> calisanId
    workersObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "create"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->workerManager || args.Length() < 1) return;

            std::string path = v8Str(iso, args[0]);
            int id = extCtx->workerManager->createWorker(path);
            args.GetReturnValue().Set(id);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.workers.createFromSource(source) -> workerId
    // editor.workers.kaynaktanOlustur(kaynak) -> calisanId
    workersObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "createFromSource"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->workerManager || args.Length() < 1) return;

            std::string source = v8Str(iso, args[0]);
            int id = extCtx->workerManager->createWorkerFromSource(source);
            args.GetReturnValue().Set(id);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.workers.postMessage(workerId, message)
    // editor.workers.mesajGonder(calisanId, mesaj)
    workersObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "postMessage"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->workerManager || args.Length() < 2) return;

            int workerId = args[0]->Int32Value(ctx2).FromMaybe(-1);
            std::string message = v8Str(iso, args[1]);
            bool ok = extCtx->workerManager->postMessage(workerId, message);
            args.GetReturnValue().Set(ok);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.workers.terminate(workerId) -> bool
    // editor.workers.sonlandir(calisanId) -> bool
    workersObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "terminate"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->workerManager || args.Length() < 1) return;

            int workerId = args[0]->Int32Value(ctx2).FromMaybe(-1);
            bool ok = extCtx->workerManager->terminate(workerId);
            args.GetReturnValue().Set(ok);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.workers.terminateAll()
    // editor.workers.tumunuSonlandir()
    workersObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "terminateAll"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->workerManager) return;
            extCtx->workerManager->terminateAll();
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.workers.state(workerId) -> "pending"|"running"|"stopped"|"error"
    // editor.workers.durum(calisanId) -> "beklemede"|"calisiyor"|"durdu"|"hata"
    workersObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "state"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->workerManager || args.Length() < 1) return;

            int workerId = args[0]->Int32Value(ctx2).FromMaybe(-1);
            WorkerState st = extCtx->workerManager->getState(workerId);
            const char* result = "stopped";
            switch (st) {
                case WorkerState::Pending: result = "pending"; break;
                case WorkerState::Running: result = "running"; break;
                case WorkerState::Error:   result = "error"; break;
                default: break;
            }
            args.GetReturnValue().Set(v8::String::NewFromUtf8(iso, result).ToLocalChecked());
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.workers.activeCount() -> number
    // editor.workers.aktifSayisi() -> sayi
    workersObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "activeCount"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->workerManager) return;
            args.GetReturnValue().Set(extCtx->workerManager->activeCount());
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.workers.onMessage(callback) - Register callback for messages from workers
    // editor.workers.mesajGeldiginde(geriCagirim) - Calisanlardan gelen mesajlar icin geri cagirim kaydet
    workersObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "onMessage"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->workerManager || args.Length() < 1 || !args[0]->IsFunction()) return;

            // Store the JS callback as a persistent handle
            // JS geri cagirimini kalici tutamak olarak sakla
            auto* persistent = new v8::Global<v8::Function>(iso, args[0].As<v8::Function>());
            auto* isoPtr = iso;

            extCtx->workerManager->setMessageCallback(
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
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
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
