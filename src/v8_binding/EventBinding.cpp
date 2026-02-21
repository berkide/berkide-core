#include "EventBinding.h"
#include "EventBus.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "Logger.h"
#include <v8.h>
#include <memory>

// Register event bus API on editor.events JS object (on, once, emit, emitSync, shutdown)
// editor.events JS nesnesine event bus API'sini kaydet (on, once, emit, emitSync, shutdown)
void RegisterEventBinding(v8::Isolate* isolate,
                          v8::Local<v8::Object> editorObj,
                          EditorContext& ctx)
{
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsEvents = v8::Object::New(isolate);
    EventBus* bus = ctx.eventBus;

    // on(eventName, callback): subscribe to an event with a persistent JS callback
    // on(eventName, callback): kalici bir JS callback ile bir olaya abone ol
    jsEvents->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "on"),
        v8::Function::New(v8ctx,
            [](const v8::FunctionCallbackInfo<v8::Value>& args)
            {
                if (args.Length() < 2) return;
                if (!args[0]->IsString() || !args[1]->IsFunction()) return;

                v8::Isolate* iso = args.GetIsolate();
                v8::String::Utf8Value evName(iso, args[0]);
                std::string name = *evName ? *evName : "";

                v8::Local<v8::Function> callback = args[1].As<v8::Function>();
                auto holder = std::make_shared<v8::Global<v8::Function>>(iso, callback);
                auto pctx = std::make_shared<v8::Global<v8::Context>>(iso, iso->GetCurrentContext());

                EventBus* eb = static_cast<EventBus*>(args.Data().As<v8::External>()->Value());
                eb->on(name,
                    [holder, pctx, iso](const EventBus::Event& e)
                    {
                        v8::Locker locker(iso);
                        v8::Isolate::Scope iso_scope(iso);
                        v8::HandleScope hs(iso);
                        auto context = pctx->Get(iso);

                        v8::Local<v8::Function> fn = holder->Get(iso);
                        v8::Local<v8::Value> argv[2];
                        argv[0] = v8::String::NewFromUtf8(iso, e.name.c_str()).ToLocalChecked();
                        argv[1] = v8::String::NewFromUtf8(iso, e.payload.c_str()).ToLocalChecked();

                        (void) fn->Call(context, context->Global(), 2, argv);
                    });
            },
            v8::External::New(isolate, bus)
        ).ToLocalChecked()
    ).Check();

    // once(eventName, callback): subscribe to an event, auto-removed after first trigger
    // once(eventName, callback): bir olaya abone ol, ilk tetiklenmeden sonra otomatik kaldir
    jsEvents->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "once"),
        v8::Function::New(v8ctx,
            [](const v8::FunctionCallbackInfo<v8::Value>& args)
            {
                if (args.Length() < 2) return;
                if (!args[0]->IsString() || !args[1]->IsFunction()) return;

                v8::Isolate* iso = args.GetIsolate();
                v8::String::Utf8Value evName(iso, args[0]);
                std::string name = *evName ? *evName : "";

                v8::Local<v8::Function> callback = args[1].As<v8::Function>();
                auto holder = std::make_shared<v8::Global<v8::Function>>(iso, callback);
                auto pctx = std::make_shared<v8::Global<v8::Context>>(iso, iso->GetCurrentContext());

                EventBus* eb = static_cast<EventBus*>(args.Data().As<v8::External>()->Value());
                eb->once(name,
                    [holder, pctx, iso](const EventBus::Event& e)
                    {
                        v8::Locker locker(iso);
                        v8::Isolate::Scope iso_scope(iso);
                        v8::HandleScope hs(iso);
                        auto context = pctx->Get(iso);

                        v8::Local<v8::Function> fn = holder->Get(iso);
                        v8::Local<v8::Value> argv[2];
                        argv[0] = v8::String::NewFromUtf8(iso, e.name.c_str()).ToLocalChecked();
                        argv[1] = v8::String::NewFromUtf8(iso, e.payload.c_str()).ToLocalChecked();

                        (void) fn->Call(context, context->Global(), 2, argv);
                        // shared_ptr auto-cleans when EventBus removes the handler
                    });
            },
            v8::External::New(isolate, bus)
        ).ToLocalChecked()
    ).Check();

    // emit(eventName, payload): fire an event asynchronously to all subscribers
    // emit(eventName, payload): tum abonelere asenkron olarak bir olay gonder
    jsEvents->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "emit"),
        v8::Function::New(v8ctx,
            [](const v8::FunctionCallbackInfo<v8::Value>& args)
            {
                if (args.Length() < 1) return;
                if (!args[0]->IsString()) return;

                v8::Isolate* iso = args.GetIsolate();
                v8::String::Utf8Value evName(iso, args[0]);
                std::string name = *evName ? *evName : "";

                std::string payload = "{}";
                if (args.Length() > 1 && args[1]->IsString()) {
                    v8::String::Utf8Value p(iso, args[1]);
                    payload = *p ? *p : "{}";
                }

                EventBus* eb = static_cast<EventBus*>(args.Data().As<v8::External>()->Value());
                eb->emit(name, payload);
            },
            v8::External::New(isolate, bus)
        ).ToLocalChecked()
    ).Check();

    // emitSync(eventName, payload): fire an event synchronously, blocking until all handlers finish
    // emitSync(eventName, payload): tum isleyiciler bitene kadar bekleyerek senkron olay gonder
    jsEvents->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "emitSync"),
        v8::Function::New(v8ctx,
            [](const v8::FunctionCallbackInfo<v8::Value>& args)
            {
                if (args.Length() < 1) return;
                if (!args[0]->IsString()) return;

                v8::Isolate* iso = args.GetIsolate();
                v8::String::Utf8Value evName(iso, args[0]);
                std::string name = *evName ? *evName : "";

                std::string payload = "{}";
                if (args.Length() > 1 && args[1]->IsString()) {
                    v8::String::Utf8Value p(iso, args[1]);
                    payload = *p ? *p : "{}";
                }

                EventBus* eb = static_cast<EventBus*>(args.Data().As<v8::External>()->Value());
                eb->emitSync(name, payload);
            },
            v8::External::New(isolate, bus)
        ).ToLocalChecked()
    ).Check();

    // shutdown(): stop the event bus and clear all listeners
    // shutdown(): event bus'i durdur ve tum dinleyicileri temizle
    jsEvents->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "shutdown"),
        v8::Function::New(v8ctx,
            [](const v8::FunctionCallbackInfo<v8::Value>& args)
            {
                EventBus* eb = static_cast<EventBus*>(args.Data().As<v8::External>()->Value());
                eb->shutdown();
            },
            v8::External::New(isolate, bus)
        ).ToLocalChecked()
    ).Check();

    // off(eventName): remove all listeners for a specific event
    // off(eventName): belirli bir olay icin tum dinleyicileri kaldir
    jsEvents->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "off"),
        v8::Function::New(v8ctx,
            [](const v8::FunctionCallbackInfo<v8::Value>& args)
            {
                if (args.Length() < 1) return;
                if (!args[0]->IsString()) return;

                v8::Isolate* iso = args.GetIsolate();
                v8::String::Utf8Value evName(iso, args[0]);
                std::string name = *evName ? *evName : "";

                EventBus* eb = static_cast<EventBus*>(args.Data().As<v8::External>()->Value());
                eb->off(name);
            },
            v8::External::New(isolate, bus)
        ).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "events"),
        jsEvents
    ).Check();

    LOG_INFO("[V8] Events API bound");
}

// Auto-register "events" binding at static init time so it is applied when editor object is created
// "events" binding'ini statik baslangicta otomatik kaydet, editor nesnesi olusturulurken uygulansin
static bool registered_events = []{
    BindingRegistry::instance().registerBinding("events",
        [](v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx){
            RegisterEventBinding(isolate, editorObj, ctx);
        });
    return true;
}();
