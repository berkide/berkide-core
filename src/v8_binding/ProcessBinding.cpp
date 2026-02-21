#include "ProcessBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "ProcessManager.h"
#include "V8Engine.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Register editor.process JS object with spawn, write, kill, signal, closeStdin, list,
// onStdout, onStderr, onExit
// editor.process JS nesnesini spawn, write, kill, signal, closeStdin, list,
// onStdout, onStderr, onExit ile kaydet
void RegisterProcessBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsProcess = v8::Object::New(isolate);
    ProcessManager* pm = ctx.processManager;

    // process.spawn(command, args?, opts?) -> processId
    // Yeni bir alt surec baslat ve surec kimligini dondur
    jsProcess->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "spawn"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            auto* pm = static_cast<ProcessManager*>(args.Data().As<v8::External>()->Value());
            if (!pm) return;
            auto* isolate = args.GetIsolate();
            auto ctx = isolate->GetCurrentContext();

            std::string command = v8Str(isolate, args[0]);

            // Parse args array
            // Args dizisini ayristir
            std::vector<std::string> cmdArgs;
            if (args.Length() > 1 && args[1]->IsArray()) {
                auto arr = args[1].As<v8::Array>();
                for (uint32_t i = 0; i < arr->Length(); ++i) {
                    auto val = arr->Get(ctx, i).ToLocalChecked();
                    cmdArgs.push_back(v8Str(isolate, val));
                }
            }

            // Parse options object
            // Secenekler nesnesini ayristir
            ProcessOptions opts;
            if (args.Length() > 2 && args[2]->IsObject()) {
                auto obj = args[2].As<v8::Object>();
                auto cwdKey = v8::String::NewFromUtf8Literal(isolate, "cwd");
                auto mergeKey = v8::String::NewFromUtf8Literal(isolate, "mergeStderr");
                auto envKey = v8::String::NewFromUtf8Literal(isolate, "env");

                if (obj->Has(ctx, cwdKey).FromMaybe(false)) {
                    opts.cwd = v8Str(isolate, obj->Get(ctx, cwdKey).ToLocalChecked());
                }
                if (obj->Has(ctx, mergeKey).FromMaybe(false)) {
                    opts.mergeStderr = obj->Get(ctx, mergeKey).ToLocalChecked()->BooleanValue(isolate);
                }
                if (obj->Has(ctx, envKey).FromMaybe(false)) {
                    auto envVal = obj->Get(ctx, envKey).ToLocalChecked();
                    if (envVal->IsArray()) {
                        auto envArr = envVal.As<v8::Array>();
                        for (uint32_t i = 0; i < envArr->Length(); ++i) {
                            opts.env.push_back(v8Str(isolate, envArr->Get(ctx, i).ToLocalChecked()));
                        }
                    }
                }
            }

            int id = pm->spawn(command, cmdArgs, opts);
            args.GetReturnValue().Set(v8::Integer::New(isolate, id));
        }, v8::External::New(isolate, pm)).ToLocalChecked()
    ).Check();

    // process.write(id, data) -> bool
    // Surecin stdin'ine veri yaz
    jsProcess->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "write"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 2) return;
            auto* pm = static_cast<ProcessManager*>(args.Data().As<v8::External>()->Value());
            if (!pm) return;
            int id = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            std::string data = v8Str(args.GetIsolate(), args[1]);
            bool ok = pm->write(id, data);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, pm)).ToLocalChecked()
    ).Check();

    // process.closeStdin(id) -> bool
    // Surecin stdin pipe'ini kapat (EOF gonder)
    jsProcess->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "closeStdin"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            auto* pm = static_cast<ProcessManager*>(args.Data().As<v8::External>()->Value());
            if (!pm) return;
            int id = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            bool ok = pm->closeStdin(id);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, pm)).ToLocalChecked()
    ).Check();

    // process.kill(id) -> bool
    // Sureci zorla oldur
    jsProcess->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "kill"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            auto* pm = static_cast<ProcessManager*>(args.Data().As<v8::External>()->Value());
            if (!pm) return;
            int id = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            bool ok = pm->kill(id);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, pm)).ToLocalChecked()
    ).Check();

    // process.signal(id, signum) -> bool
    // Surece sinyal gonder (orn: 15=SIGTERM, 9=SIGKILL)
    jsProcess->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "signal"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 2) return;
            auto* pm = static_cast<ProcessManager*>(args.Data().As<v8::External>()->Value());
            if (!pm) return;
            auto ctx = args.GetIsolate()->GetCurrentContext();
            int id  = args[0]->Int32Value(ctx).FromMaybe(0);
            int sig = args[1]->Int32Value(ctx).FromMaybe(15);
            bool ok = pm->signal(id, sig);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, pm)).ToLocalChecked()
    ).Check();

    // process.isRunning(id) -> bool
    // Surecin calismakta olup olmadigini kontrol et
    jsProcess->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isRunning"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            auto* pm = static_cast<ProcessManager*>(args.Data().As<v8::External>()->Value());
            if (!pm) return;
            int id = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), pm->isRunning(id)));
        }, v8::External::New(isolate, pm)).ToLocalChecked()
    ).Check();

    // process.list() -> [{id, pid, running, exitCode}]
    // Tum surecleri listele
    jsProcess->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* pm = static_cast<ProcessManager*>(args.Data().As<v8::External>()->Value());
            if (!pm) return;
            auto* isolate = args.GetIsolate();
            auto ctx = isolate->GetCurrentContext();

            auto procs = pm->list();
            v8::Local<v8::Array> arr = v8::Array::New(isolate, static_cast<int>(procs.size()));
            for (size_t i = 0; i < procs.size(); ++i) {
                v8::Local<v8::Object> obj = v8::Object::New(isolate);
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "id"),
                    v8::Integer::New(isolate, procs[i].id)).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "pid"),
                    v8::Integer::New(isolate, static_cast<int>(procs[i].pid))).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "running"),
                    v8::Boolean::New(isolate, procs[i].running)).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "exitCode"),
                    v8::Integer::New(isolate, procs[i].exitCode)).Check();
                arr->Set(ctx, static_cast<uint32_t>(i), obj).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, pm)).ToLocalChecked()
    ).Check();

    // process.onStdout(id, callback) - Set stdout callback
    // Stdout verisi icin geri cagirim ayarla
    jsProcess->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "onStdout"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 2 || !args[1]->IsFunction()) return;
            auto* pm = static_cast<ProcessManager*>(args.Data().As<v8::External>()->Value());
            if (!pm) return;
            auto* isolate = args.GetIsolate();
            int id = args[0]->Int32Value(isolate->GetCurrentContext()).FromMaybe(0);

            auto cb = std::make_shared<v8::Global<v8::Function>>(isolate, args[1].As<v8::Function>());
            auto ctxG = std::make_shared<v8::Global<v8::Context>>(isolate, isolate->GetCurrentContext());

            pm->onStdout(id, [cb, ctxG, isolate](int procId, const std::string& data) {
                auto* engine = static_cast<V8Engine*>(isolate->GetData(0));
                struct Task : v8::Task {
                    v8::Isolate* iso;
                    V8Engine* eng;
                    std::shared_ptr<v8::Global<v8::Function>> fn;
                    std::shared_ptr<v8::Global<v8::Context>> ctx;
                    std::string data;
                    int id;
                    void Run() override {
                        v8::Isolate::Scope isoScope(iso);
                        v8::HandleScope hs(iso);
                        auto context = ctx->Get(iso);
                        v8::Context::Scope cs(context);
                        v8::Local<v8::Value> argv[1] = {
                            v8::String::NewFromUtf8(iso, data.c_str()).ToLocalChecked()
                        };
                        (void)fn->Get(iso)->Call(context, context->Global(), 1, argv);
                    }
                };
                auto task = std::make_unique<Task>();
                task->iso = isolate;
                task->eng = engine;
                task->fn = cb;
                task->ctx = ctxG;
                task->data = data;
                task->id = procId;
                engine->platform()->GetForegroundTaskRunner(isolate)->PostTask(std::move(task));
            });
        }, v8::External::New(isolate, pm)).ToLocalChecked()
    ).Check();

    // process.onStderr(id, callback) - Set stderr callback
    // Stderr verisi icin geri cagirim ayarla
    jsProcess->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "onStderr"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 2 || !args[1]->IsFunction()) return;
            auto* pm = static_cast<ProcessManager*>(args.Data().As<v8::External>()->Value());
            if (!pm) return;
            auto* isolate = args.GetIsolate();
            int id = args[0]->Int32Value(isolate->GetCurrentContext()).FromMaybe(0);

            auto cb = std::make_shared<v8::Global<v8::Function>>(isolate, args[1].As<v8::Function>());
            auto ctxG = std::make_shared<v8::Global<v8::Context>>(isolate, isolate->GetCurrentContext());

            pm->onStderr(id, [cb, ctxG, isolate](int procId, const std::string& data) {
                auto* engine = static_cast<V8Engine*>(isolate->GetData(0));
                struct Task : v8::Task {
                    v8::Isolate* iso;
                    std::shared_ptr<v8::Global<v8::Function>> fn;
                    std::shared_ptr<v8::Global<v8::Context>> ctx;
                    std::string data;
                    void Run() override {
                        v8::Isolate::Scope isoScope(iso);
                        v8::HandleScope hs(iso);
                        auto context = ctx->Get(iso);
                        v8::Context::Scope cs(context);
                        v8::Local<v8::Value> argv[1] = {
                            v8::String::NewFromUtf8(iso, data.c_str()).ToLocalChecked()
                        };
                        (void)fn->Get(iso)->Call(context, context->Global(), 1, argv);
                    }
                };
                auto task = std::make_unique<Task>();
                task->iso = isolate;
                task->fn = cb;
                task->ctx = ctxG;
                task->data = data;
                engine->platform()->GetForegroundTaskRunner(isolate)->PostTask(std::move(task));
            });
        }, v8::External::New(isolate, pm)).ToLocalChecked()
    ).Check();

    // process.onExit(id, callback) - Set exit callback
    // Surec cikisi icin geri cagirim ayarla
    jsProcess->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "onExit"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 2 || !args[1]->IsFunction()) return;
            auto* pm = static_cast<ProcessManager*>(args.Data().As<v8::External>()->Value());
            if (!pm) return;
            auto* isolate = args.GetIsolate();
            int id = args[0]->Int32Value(isolate->GetCurrentContext()).FromMaybe(0);

            auto cb = std::make_shared<v8::Global<v8::Function>>(isolate, args[1].As<v8::Function>());
            auto ctxG = std::make_shared<v8::Global<v8::Context>>(isolate, isolate->GetCurrentContext());

            pm->onExit(id, [cb, ctxG, isolate](int procId, int exitCode) {
                auto* engine = static_cast<V8Engine*>(isolate->GetData(0));
                struct Task : v8::Task {
                    v8::Isolate* iso;
                    std::shared_ptr<v8::Global<v8::Function>> fn;
                    std::shared_ptr<v8::Global<v8::Context>> ctx;
                    int exitCode;
                    void Run() override {
                        v8::Isolate::Scope isoScope(iso);
                        v8::HandleScope hs(iso);
                        auto context = ctx->Get(iso);
                        v8::Context::Scope cs(context);
                        v8::Local<v8::Value> argv[1] = {
                            v8::Integer::New(iso, exitCode)
                        };
                        (void)fn->Get(iso)->Call(context, context->Global(), 1, argv);
                    }
                };
                auto task = std::make_unique<Task>();
                task->iso = isolate;
                task->fn = cb;
                task->ctx = ctxG;
                task->exitCode = exitCode;
                engine->platform()->GetForegroundTaskRunner(isolate)->PostTask(std::move(task));
            });
        }, v8::External::New(isolate, pm)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "process"),
        jsProcess).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _processReg = [] {
    BindingRegistry::instance().registerBinding("process", RegisterProcessBinding);
    return true;
}();
