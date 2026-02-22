// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "CursorBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "buffers.h"
#include <v8.h>

// Context struct to pass both buffers pointer and i18n to lambda callbacks
// Lambda callback'lere hem buffers hem i18n isaretcisini aktarmak icin baglam yapisi
struct CursorCtx {
    Buffers* bufs;
    I18n* i18n;
};

// Register cursor API on editor.cursor JS object (getLine, getCol, setPosition, moveUp/Down/Left/Right, etc.)
// editor.cursor JS nesnesine cursor API'sini kaydet (getLine, getCol, setPosition, moveUp/Down/Left/Right, vb.)
void RegisterCursorBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsCursor = v8::Object::New(isolate);

    auto* cctx = new CursorCtx{ctx.buffers, ctx.i18n};

    // cursor.getLine() -> {ok, data: number, ...}
    // Imlecin bulundugu satir numarasini dondur
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getLine"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* cc = static_cast<CursorCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, cc ? cc->i18n : nullptr);
                return;
            }
            int line = cc->bufs->active().getCursor().getLine();
            V8Response::ok(args, line);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // cursor.getCol() -> {ok, data: number, ...}
    // Imlecin bulundugu sutun numarasini dondur
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getCol"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* cc = static_cast<CursorCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, cc ? cc->i18n : nullptr);
                return;
            }
            int col = cc->bufs->active().getCursor().getCol();
            V8Response::ok(args, col);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // cursor.setPosition(line, col) -> {ok, data: true, ...}
    // Imleci belirtilen satir ve sutuna tasi
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setPosition"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* cc = static_cast<CursorCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "line, col"}}, cc->i18n);
                return;
            }
            int line = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int col  = args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            cc->bufs->active().getCursor().setPosition(line, col);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // cursor.moveUp() -> {ok, data: true, ...}
    // Imleci bir satir yukari tasi
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveUp"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* cc = static_cast<CursorCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, cc ? cc->i18n : nullptr);
                return;
            }
            cc->bufs->active().getCursor().moveUp(cc->bufs->active().getBuffer());
            V8Response::ok(args, true);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // cursor.moveDown() -> {ok, data: true, ...}
    // Imleci bir satir asagi tasi
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveDown"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* cc = static_cast<CursorCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, cc ? cc->i18n : nullptr);
                return;
            }
            cc->bufs->active().getCursor().moveDown(cc->bufs->active().getBuffer());
            V8Response::ok(args, true);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // cursor.moveLeft() -> {ok, data: true, ...}
    // Imleci bir karakter sola tasi
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveLeft"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* cc = static_cast<CursorCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, cc ? cc->i18n : nullptr);
                return;
            }
            cc->bufs->active().getCursor().moveLeft(cc->bufs->active().getBuffer());
            V8Response::ok(args, true);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // cursor.moveRight() -> {ok, data: true, ...}
    // Imleci bir karakter saga tasi
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveRight"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* cc = static_cast<CursorCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, cc ? cc->i18n : nullptr);
                return;
            }
            cc->bufs->active().getCursor().moveRight(cc->bufs->active().getBuffer());
            V8Response::ok(args, true);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // cursor.moveToLineEnd() -> {ok, data: true, ...}
    // Imleci satir sonuna tasi
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveToLineEnd"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* cc = static_cast<CursorCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, cc ? cc->i18n : nullptr);
                return;
            }
            cc->bufs->active().getCursor().moveToLineEnd(cc->bufs->active().getBuffer());
            V8Response::ok(args, true);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // cursor.clampToBuffer() -> {ok, data: true, ...}
    // Imleci buffer sinirlarina sabitle
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clampToBuffer"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* cc = static_cast<CursorCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, cc ? cc->i18n : nullptr);
                return;
            }
            cc->bufs->active().getCursor().clampToBuffer(cc->bufs->active().getBuffer());
            V8Response::ok(args, true);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // cursor.moveToLineStart(): move cursor to the beginning of the current line
    // cursor.moveToLineStart(): imleci mevcut satirin basina tasi
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveToLineStart"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* cc = static_cast<CursorCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, cc ? cc->i18n : nullptr);
                return;
            }
            auto& cursor = cc->bufs->active().getCursor();
            cursor.setPosition(cursor.getLine(), 0);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx, v8::String::NewFromUtf8Literal(isolate, "cursor"), jsCursor).Check();
}

// Auto-register "cursor" binding at static init time so it is applied when editor object is created
// "cursor" binding'ini statik baslangicta otomatik kaydet, editor nesnesi olusturulurken uygulansin
static bool registered_cursor = []{
    BindingRegistry::instance().registerBinding("cursor",
        [](v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx){
            RegisterCursorBinding(isolate, editorObj, ctx);
        });
    return true;
}();
