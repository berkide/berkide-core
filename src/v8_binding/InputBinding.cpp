// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "InputBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "input.h"
#include <v8.h>
#include <unordered_map>
#include <memory>

// Context struct to pass both input handler and i18n to lambda callbacks
// Lambda callback'lere hem input handler hem i18n isaretcisini aktarmak icin baglam yapisi
struct InputCtx {
    InputHandler* input;
    I18n* i18n;
};

// Register input API on editor.input JS object (registerOnKeyDown, registerOnCharInput, bindChord, start, stop)
// editor.input JS nesnesine input API'sini kaydet (registerOnKeyDown, registerOnCharInput, bindChord, start, stop)
void RegisterInputBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsInput = v8::Object::New(isolate);

    auto* ictx = new InputCtx{ctx.input, ctx.i18n};

    // input.registerOnKeyDown(fn): register a JS callback for all key-down events with event details
    // input.registerOnKeyDown(fn): tum tus-basildi olaylari icin detayli olay bilgisiyle JS callback kaydet
    jsInput->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "registerOnKeyDown"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* ic = static_cast<InputCtx*>(args.Data().As<v8::External>()->Value());
            if (!ic || !ic->input) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, ic ? ic->i18n : nullptr);
                return;
            }
            if (args.Length() < 1 || !args[0]->IsFunction()) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "fn"}}, ic->i18n);
                return;
            }

            auto* input = ic->input;
            auto isolate = args.GetIsolate();

            // Use shared_ptr so the lambda is copyable
            // Lambda'nin kopyalanabilir olmasi icin shared_ptr kullan
            auto callback = std::make_shared<v8::Global<v8::Function>>(isolate, v8::Local<v8::Function>::Cast(args[0]));

            input->setOnKeyDown([isolate, callback](const InputHandler::KeyEvent& ev) {
                v8::HandleScope scope(isolate);
                auto ctx = isolate->GetCurrentContext();
                auto fn = callback->Get(isolate);

                auto obj = v8::Object::New(isolate);
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "text"),
                         v8::String::NewFromUtf8(isolate, ev.text.c_str()).ToLocalChecked()).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "ctrl"),
                         v8::Boolean::New(isolate, ev.ctrl)).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "alt"),
                         v8::Boolean::New(isolate, ev.alt)).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "shift"),
                         v8::Boolean::New(isolate, ev.shift)).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "isChar"),
                         v8::Boolean::New(isolate, ev.isChar)).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "chord"),
                         v8::String::NewFromUtf8(isolate, InputHandler::toChordString(ev).c_str()).ToLocalChecked()).Check();

                v8::Local<v8::Value> argv[1] = { obj };
                (void)fn->Call(ctx, v8::Undefined(isolate), 1, argv);
            });
            V8Response::ok(args, true);
        }, v8::External::New(isolate, ictx)).ToLocalChecked()
    ).Check();

    // input.registerOnCharInput(fn): register a JS callback for printable character input
    // input.registerOnCharInput(fn): yazilabilir karakter girisi icin JS callback kaydet
    jsInput->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "registerOnCharInput"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* ic = static_cast<InputCtx*>(args.Data().As<v8::External>()->Value());
            if (!ic || !ic->input) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, ic ? ic->i18n : nullptr);
                return;
            }
            if (args.Length() < 1 || !args[0]->IsFunction()) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "fn"}}, ic->i18n);
                return;
            }

            auto* input = ic->input;
            auto isolate = args.GetIsolate();

            auto callback = std::make_shared<v8::Global<v8::Function>>(isolate, v8::Local<v8::Function>::Cast(args[0]));

            input->setOnCharInput([isolate, callback](const InputHandler::KeyEvent& ev) {
                v8::HandleScope scope(isolate);
                auto ctx = isolate->GetCurrentContext();
                auto fn = callback->Get(isolate);
                v8::Local<v8::Value> argv[1] = {
                    v8::String::NewFromUtf8(isolate, ev.text.c_str()).ToLocalChecked()
                };
                (void)fn->Call(ctx, v8::Undefined(isolate), 1, argv);
            });
            V8Response::ok(args, true);
        }, v8::External::New(isolate, ictx)).ToLocalChecked()
    ).Check();

    // input.bindChord(chord, fn): bind a keyboard shortcut (e.g. "Ctrl+S") to a JS callback
    // input.bindChord(chord, fn): bir klavye kisayolunu (orn. "Ctrl+S") JS callback'e bagla
    jsInput->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "bindChord"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* ic = static_cast<InputCtx*>(args.Data().As<v8::External>()->Value());
            if (!ic || !ic->input) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, ic ? ic->i18n : nullptr);
                return;
            }
            if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsFunction()) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "chord, fn"}}, ic->i18n);
                return;
            }

            auto* input = ic->input;
            auto isolate = args.GetIsolate();

            v8::String::Utf8Value chord(isolate, args[0]);
            std::string key(*chord);

            auto callback = std::make_shared<v8::Global<v8::Function>>(isolate, v8::Local<v8::Function>::Cast(args[1]));

            input->bindChord(key, [isolate, key, callback](const InputHandler::KeyEvent& ev) {
                v8::HandleScope scope(isolate);
                auto ctx = isolate->GetCurrentContext();
                auto fn = callback->Get(isolate);
                v8::Local<v8::Value> argv[1] = {
                    v8::String::NewFromUtf8(isolate, key.c_str()).ToLocalChecked()
                };
                (void)fn->Call(ctx, v8::Undefined(isolate), 1, argv);
            });
            V8Response::ok(args, true);
        }, v8::External::New(isolate, ictx)).ToLocalChecked()
    ).Check();

    // input.start() -> {ok, data: true, ...}
    // Input dinleme thread'ini baslat
    jsInput->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "start"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* ic = static_cast<InputCtx*>(args.Data().As<v8::External>()->Value());
            if (!ic || !ic->input) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, ic ? ic->i18n : nullptr);
                return;
            }
            std::thread([ic] { ic->input->start(); }).detach();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, ictx)).ToLocalChecked()
    ).Check();

    // input.stop() -> {ok, data: true, ...}
    // Input dinleme thread'ini durdur
    jsInput->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "stop"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* ic = static_cast<InputCtx*>(args.Data().As<v8::External>()->Value());
            if (!ic || !ic->input) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, ic ? ic->i18n : nullptr);
                return;
            }
            ic->input->stop();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, ictx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx, v8::String::NewFromUtf8Literal(isolate, "input"), jsInput).Check();
}

// Auto-register "input" binding at static init time so it is applied when editor object is created
// "input" binding'ini statik baslangicta otomatik kaydet, editor nesnesi olusturulurken uygulansin
static bool registered_input = []{
    BindingRegistry::instance().registerBinding("input",
        [](v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx){
            RegisterInputBinding(isolate, editorObj, ctx);
        });
    return true;
}();
