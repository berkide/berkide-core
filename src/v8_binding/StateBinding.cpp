// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "StateBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "buffers.h"
#include <v8.h>

// Context struct to pass both buffers pointer and i18n to lambda callbacks
// Lambda callback'lere hem buffers hem i18n isaretcisini aktarmak icin baglam yapisi
struct StateCtx {
    Buffers* bufs;
    I18n* i18n;
};

// Register state API on editor.state JS object (getMode, setMode, isModified, filePath, markModified, reset)
// editor.state JS nesnesine state API'sini kaydet (getMode, setMode, isModified, filePath, markModified, reset)
void RegisterStateBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx)
{
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsState = v8::Object::New(isolate);

    auto* sctx = new StateCtx{ctx.buffers, ctx.i18n};

    // state.getMode() -> {ok, data: "normal"|"insert"|"visual", ...}
    // Mevcut duzenleme modunu dondur
    jsState->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getMode"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* sc = static_cast<StateCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, sc ? sc->i18n : nullptr);
                return;
            }
            EditMode mode = sc->bufs->active().getMode();
            std::string m = "normal";
            if (mode == EditMode::Insert) m = "insert";
            else if (mode == EditMode::Visual) m = "visual";
            V8Response::ok(args, m);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // state.setMode(modeStr) -> {ok, data: true, ...}
    // Duzenleme modunu ayarla
    jsState->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setMode"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* sc = static_cast<StateCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, sc ? sc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "modeStr"}}, sc->i18n);
                return;
            }
            v8::String::Utf8Value s(args.GetIsolate(), args[0]);
            EditMode m = EditMode::Normal;
            if (std::string(*s) == "insert") m = EditMode::Insert;
            else if (std::string(*s) == "visual") m = EditMode::Visual;
            sc->bufs->active().setMode(m);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // state.isModified() -> {ok, data: bool, ...}
    // Buffer'in degistirilip degistirilmedigini kontrol et
    jsState->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isModified"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* sc = static_cast<StateCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, sc ? sc->i18n : nullptr);
                return;
            }
            bool modified = sc->bufs->active().isModified();
            V8Response::ok(args, modified);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // state.filePath() -> {ok, data: "path/to/file", ...}
    // Mevcut belgenin dosya yolunu dondur
    jsState->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "filePath"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* sc = static_cast<StateCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, sc ? sc->i18n : nullptr);
                return;
            }
            std::string p = sc->bufs->active().getFilePath();
            V8Response::ok(args, p);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // state.markModified(bool) -> {ok, data: true, ...}
    // Buffer'i degistirilmis olarak isaretle
    jsState->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "markModified"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* sc = static_cast<StateCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, sc ? sc->i18n : nullptr);
                return;
            }
            bool b = (args.Length() > 0) ? args[0]->BooleanValue(args.GetIsolate()) : true;
            sc->bufs->active().markModified(b);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // state.reset() -> {ok, data: true, ...}
    // Buffer durumunu sifirla
    jsState->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "reset"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* sc = static_cast<StateCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, sc ? sc->i18n : nullptr);
                return;
            }
            sc->bufs->active().reset();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // state.setFilePath(path) - Set the file path for current document
    // Mevcut belge icin dosya yolunu ayarla
    jsState->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setFilePath"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* sc = static_cast<StateCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, sc ? sc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "path"}}, sc->i18n);
                return;
            }
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            sc->bufs->active().setFilePath(*path);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx, v8::String::NewFromUtf8Literal(isolate, "state"), jsState).Check();
}

// Auto-register "state" binding at static init time so it is applied when editor object is created
// "state" binding'ini statik baslangicta otomatik kaydet, editor nesnesi olusturulurken uygulansin
static bool registered_state = []{
    BindingRegistry::instance().registerBinding("state",
        [](v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx){
            RegisterStateBinding(isolate, editorObj, ctx);
        });
    return true;
}();
