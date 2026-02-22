// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "EventBinding.h"
#include "EventBus.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "Logger.h"
#include <v8.h>
#include <memory>

// Context struct to pass event bus pointer and i18n to lambda callbacks
// Lambda callback'lere hem event bus hem i18n isaretcisini aktarmak icin baglam yapisi
struct EventCtx {
    EventBus* bus;
    I18n* i18n;
};

// Register event bus API on editor.events JS object (on, once, emit, emitSync, shutdown)
// editor.events JS nesnesine event bus API'sini kaydet (on, once, emit, emitSync, shutdown)
void RegisterEventBinding(v8::Isolate* isolate,
                          v8::Local<v8::Object> editorObj,
                          EditorContext& ctx)
{
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsEvents = v8::Object::New(isolate);

    auto* ectx = new EventCtx{ctx.eventBus, ctx.i18n};

    // on(eventName, callback): subscribe to an event with a persistent JS callback
    // on(eventName, callback): kalici bir JS callback ile bir olaya abone ol
    jsEvents->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "on"),
        v8::Function::New(v8ctx,
            [](const v8::FunctionCallbackInfo<v8::Value>& args)
            {
                auto* ec = static_cast<EventCtx*>(args.Data().As<v8::External>()->Value());
                if (!ec || !ec->bus) {
                    V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, ec ? ec->i18n : nullptr);
                    return;
                }
                if (args.Length() < 2) {
                    V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "eventName, callback"}}, ec->i18n);
                    return;
                }
                if (!args[0]->IsString() || !args[1]->IsFunction()) {
                    V8Response::error(args, "INVALID_ARG", "args.invalid_type", {{"name", "eventName, callback"}}, ec->i18n);
                    return;
                }

                v8::Isolate* iso = args.GetIsolate();
                v8::String::Utf8Value evName(iso, args[0]);
                std::string name = *evName ? *evName : "";

                v8::Local<v8::Function> callback = args[1].As<v8::Function>();
                auto holder = std::make_shared<v8::Global<v8::Function>>(iso, callback);
                auto pctx = std::make_shared<v8::Global<v8::Context>>(iso, iso->GetCurrentContext());

                // Listener callback remains raw (fire-and-forget)
                // Dinleyici callback'i ham kalir (atesle-ve-unut)
                ec->bus->on(name,
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

                V8Response::ok(args, true);
            },
            v8::External::New(isolate, ectx)
        ).ToLocalChecked()
    ).Check();

    // once(eventName, callback): subscribe to an event, auto-removed after first trigger
    // once(eventName, callback): bir olaya abone ol, ilk tetiklenmeden sonra otomatik kaldir
    jsEvents->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "once"),
        v8::Function::New(v8ctx,
            [](const v8::FunctionCallbackInfo<v8::Value>& args)
            {
                auto* ec = static_cast<EventCtx*>(args.Data().As<v8::External>()->Value());
                if (!ec || !ec->bus) {
                    V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, ec ? ec->i18n : nullptr);
                    return;
                }
                if (args.Length() < 2) {
                    V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "eventName, callback"}}, ec->i18n);
                    return;
                }
                if (!args[0]->IsString() || !args[1]->IsFunction()) {
                    V8Response::error(args, "INVALID_ARG", "args.invalid_type", {{"name", "eventName, callback"}}, ec->i18n);
                    return;
                }

                v8::Isolate* iso = args.GetIsolate();
                v8::String::Utf8Value evName(iso, args[0]);
                std::string name = *evName ? *evName : "";

                v8::Local<v8::Function> callback = args[1].As<v8::Function>();
                auto holder = std::make_shared<v8::Global<v8::Function>>(iso, callback);
                auto pctx = std::make_shared<v8::Global<v8::Context>>(iso, iso->GetCurrentContext());

                // Listener callback remains raw (fire-and-forget)
                // Dinleyici callback'i ham kalir (atesle-ve-unut)
                ec->bus->once(name,
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

                V8Response::ok(args, true);
            },
            v8::External::New(isolate, ectx)
        ).ToLocalChecked()
    ).Check();

    // emit(eventName, payload): fire an event asynchronously to all subscribers
    // emit(eventName, payload): tum abonelere asenkron olarak bir olay gonder
    jsEvents->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "emit"),
        v8::Function::New(v8ctx,
            [](const v8::FunctionCallbackInfo<v8::Value>& args)
            {
                auto* ec = static_cast<EventCtx*>(args.Data().As<v8::External>()->Value());
                if (!ec || !ec->bus) {
                    V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, ec ? ec->i18n : nullptr);
                    return;
                }
                if (args.Length() < 1) {
                    V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "eventName"}}, ec->i18n);
                    return;
                }
                if (!args[0]->IsString()) {
                    V8Response::error(args, "INVALID_ARG", "args.invalid_type", {{"name", "eventName"}}, ec->i18n);
                    return;
                }

                v8::Isolate* iso = args.GetIsolate();
                v8::String::Utf8Value evName(iso, args[0]);
                std::string name = *evName ? *evName : "";

                std::string payload = "{}";
                if (args.Length() > 1 && args[1]->IsString()) {
                    v8::String::Utf8Value p(iso, args[1]);
                    payload = *p ? *p : "{}";
                }

                ec->bus->emit(name, payload);
                V8Response::ok(args, true);
            },
            v8::External::New(isolate, ectx)
        ).ToLocalChecked()
    ).Check();

    // emitSync(eventName, payload): fire an event synchronously, blocking until all handlers finish
    // emitSync(eventName, payload): tum isleyiciler bitene kadar bekleyerek senkron olay gonder
    jsEvents->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "emitSync"),
        v8::Function::New(v8ctx,
            [](const v8::FunctionCallbackInfo<v8::Value>& args)
            {
                auto* ec = static_cast<EventCtx*>(args.Data().As<v8::External>()->Value());
                if (!ec || !ec->bus) {
                    V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, ec ? ec->i18n : nullptr);
                    return;
                }
                if (args.Length() < 1) {
                    V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "eventName"}}, ec->i18n);
                    return;
                }
                if (!args[0]->IsString()) {
                    V8Response::error(args, "INVALID_ARG", "args.invalid_type", {{"name", "eventName"}}, ec->i18n);
                    return;
                }

                v8::Isolate* iso = args.GetIsolate();
                v8::String::Utf8Value evName(iso, args[0]);
                std::string name = *evName ? *evName : "";

                std::string payload = "{}";
                if (args.Length() > 1 && args[1]->IsString()) {
                    v8::String::Utf8Value p(iso, args[1]);
                    payload = *p ? *p : "{}";
                }

                ec->bus->emitSync(name, payload);
                V8Response::ok(args, true);
            },
            v8::External::New(isolate, ectx)
        ).ToLocalChecked()
    ).Check();

    // shutdown(): stop the event bus and clear all listeners
    // shutdown(): event bus'i durdur ve tum dinleyicileri temizle
    jsEvents->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "shutdown"),
        v8::Function::New(v8ctx,
            [](const v8::FunctionCallbackInfo<v8::Value>& args)
            {
                auto* ec = static_cast<EventCtx*>(args.Data().As<v8::External>()->Value());
                if (!ec || !ec->bus) {
                    V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, ec ? ec->i18n : nullptr);
                    return;
                }
                ec->bus->shutdown();
                V8Response::ok(args, true);
            },
            v8::External::New(isolate, ectx)
        ).ToLocalChecked()
    ).Check();

    // off(eventName): remove all listeners for a specific event
    // off(eventName): belirli bir olay icin tum dinleyicileri kaldir
    jsEvents->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "off"),
        v8::Function::New(v8ctx,
            [](const v8::FunctionCallbackInfo<v8::Value>& args)
            {
                auto* ec = static_cast<EventCtx*>(args.Data().As<v8::External>()->Value());
                if (!ec || !ec->bus) {
                    V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, ec ? ec->i18n : nullptr);
                    return;
                }
                if (args.Length() < 1) {
                    V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "eventName"}}, ec->i18n);
                    return;
                }
                if (!args[0]->IsString()) {
                    V8Response::error(args, "INVALID_ARG", "args.invalid_type", {{"name", "eventName"}}, ec->i18n);
                    return;
                }

                v8::Isolate* iso = args.GetIsolate();
                v8::String::Utf8Value evName(iso, args[0]);
                std::string name = *evName ? *evName : "";

                ec->bus->off(name);
                V8Response::ok(args, true);
            },
            v8::External::New(isolate, ectx)
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
