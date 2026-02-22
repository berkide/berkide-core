// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "UndoBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "buffers.h"
#include "undo.h"
#include <v8.h>

// Context struct to pass both buffers pointer and i18n to lambda callbacks
// Lambda callback'lere hem buffers hem i18n isaretcisini aktarmak icin baglam yapisi
struct UndoCtx {
    Buffers* bufs;
    I18n* i18n;
};

// Register undo/redo API on editor.undo JS object (addAction, undo, redo)
// editor.undo JS nesnesine geri al/yinele API'sini kaydet (addAction, undo, redo)
void RegisterUndoBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsUndo = v8::Object::New(isolate);

    auto* uctx = new UndoCtx{ctx.buffers, ctx.i18n};

    // undo.addAction(type, line, col, char, lineContent) -> {ok, data: true, ...}
    // Geri alma yiginina yeni bir eylem ekle
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "addAction"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* uc = static_cast<UndoCtx*>(args.Data().As<v8::External>()->Value());
            if (!uc || !uc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, uc ? uc->i18n : nullptr);
                return;
            }
            if (args.Length() < 3) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "type, line, col"}}, uc->i18n);
                return;
            }
            int typeInt = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int line = args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int col  = args[2]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);

            char ch = '\0';
            std::string lineContent;

            if (args.Length() >= 4) {
                v8::String::Utf8Value cstr(args.GetIsolate(), args[3]);
                if (cstr.length() > 0) ch = (*cstr)[0];
            }
            if (args.Length() >= 5) {
                v8::String::Utf8Value lstr(args.GetIsolate(), args[4]);
                lineContent = *lstr;
            }

            Action action;
            action.type = static_cast<ActionType>(typeInt);
            action.line = line;
            action.col = col;
            action.character = ch;
            action.lineContent = lineContent;

            uc->bufs->active().getUndo().addAction(action);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, uctx)).ToLocalChecked()
    ).Check();

    // undo.undo() -> {ok, data: bool, ...}
    // Son eylemi geri al
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "undo"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* uc = static_cast<UndoCtx*>(args.Data().As<v8::External>()->Value());
            if (!uc || !uc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, uc ? uc->i18n : nullptr);
                return;
            }
            bool ok = uc->bufs->active().getUndo().undo(uc->bufs->active().getBuffer());
            V8Response::ok(args, ok);
        }, v8::External::New(isolate, uctx)).ToLocalChecked()
    ).Check();

    // undo.redo() -> {ok, data: bool, ...}
    // Son geri alinan eylemi yinele
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "redo"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* uc = static_cast<UndoCtx*>(args.Data().As<v8::External>()->Value());
            if (!uc || !uc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, uc ? uc->i18n : nullptr);
                return;
            }
            bool ok = uc->bufs->active().getUndo().redo(uc->bufs->active().getBuffer());
            V8Response::ok(args, ok);
        }, v8::External::New(isolate, uctx)).ToLocalChecked()
    ).Check();

    // undo.beginGroup(): begin a group of actions that undo/redo as a single step
    // undo.beginGroup(): tek adim olarak geri alinacak/yinelenecek bir eylem grubu baslat
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "beginGroup"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* uc = static_cast<UndoCtx*>(args.Data().As<v8::External>()->Value());
            if (!uc || !uc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, uc ? uc->i18n : nullptr);
                return;
            }
            uc->bufs->active().getUndo().beginGroup();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, uctx)).ToLocalChecked()
    ).Check();

    // undo.endGroup(): end the current action group
    // undo.endGroup(): mevcut eylem grubunu bitir
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "endGroup"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* uc = static_cast<UndoCtx*>(args.Data().As<v8::External>()->Value());
            if (!uc || !uc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, uc ? uc->i18n : nullptr);
                return;
            }
            uc->bufs->active().getUndo().endGroup();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, uctx)).ToLocalChecked()
    ).Check();

    // undo.inGroup(): check if currently inside an undo group
    // undo.inGroup(): su an bir geri alma grubu icinde olup olmadigini kontrol et
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "inGroup"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* uc = static_cast<UndoCtx*>(args.Data().As<v8::External>()->Value());
            if (!uc || !uc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, uc ? uc->i18n : nullptr);
                return;
            }
            bool result = uc->bufs->active().getUndo().inGroup();
            V8Response::ok(args, result);
        }, v8::External::New(isolate, uctx)).ToLocalChecked()
    ).Check();

    // undo.branch(index): switch to a different branch at the current undo node
    // undo.branch(index): mevcut geri alma dugumunde farkli bir dala gec
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "branch"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* uc = static_cast<UndoCtx*>(args.Data().As<v8::External>()->Value());
            if (!uc || !uc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, uc ? uc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "index"}}, uc->i18n);
                return;
            }
            int index = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            uc->bufs->active().getUndo().branch(index);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, uctx)).ToLocalChecked()
    ).Check();

    // undo.branchCount(): get the number of branches at the current undo node
    // undo.branchCount(): mevcut geri alma dugumundeki dal sayisini al
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "branchCount"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* uc = static_cast<UndoCtx*>(args.Data().As<v8::External>()->Value());
            if (!uc || !uc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, uc ? uc->i18n : nullptr);
                return;
            }
            int count = uc->bufs->active().getUndo().branchCount();
            V8Response::ok(args, count);
        }, v8::External::New(isolate, uctx)).ToLocalChecked()
    ).Check();

    // undo.currentBranch(): get the active branch index at the current undo node
    // undo.currentBranch(): mevcut geri alma dugumundeki aktif dal indeksini al
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "currentBranch"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* uc = static_cast<UndoCtx*>(args.Data().As<v8::External>()->Value());
            if (!uc || !uc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, uc ? uc->i18n : nullptr);
                return;
            }
            int index = uc->bufs->active().getUndo().currentBranch();
            V8Response::ok(args, index);
        }, v8::External::New(isolate, uctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx, v8::String::NewFromUtf8Literal(isolate, "undo"), jsUndo).Check();
}

// Auto-register "undo" binding at static init time so it is applied when editor object is created
// "undo" binding'ini statik baslangicta otomatik kaydet, editor nesnesi olusturulurken uygulansin
static bool registered_undo = []{
    BindingRegistry::instance().registerBinding("undo",
        [](v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx){
            RegisterUndoBinding(isolate, editorObj, ctx);
        });
    return true;
}();
