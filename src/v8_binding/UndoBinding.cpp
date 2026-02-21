#include "UndoBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "buffers.h"
#include "undo.h"
#include <v8.h>

// Register undo/redo API on editor.undo JS object (addAction, undo, redo)
// editor.undo JS nesnesine geri al/yinele API'sini kaydet (addAction, undo, redo)
void RegisterUndoBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsUndo = v8::Object::New(isolate);
    Buffers* buffers = ctx.buffers;

    // undo.addAction(type, line, col, char, lineContent)
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "addAction"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 3) return;
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

            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getUndo().addAction(action);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // undo.undo()
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "undo"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bool ok = bufs->active().getUndo().undo(bufs->active().getBuffer());
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // undo.redo()
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "redo"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bool ok = bufs->active().getUndo().redo(bufs->active().getBuffer());
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // undo.beginGroup(): begin a group of actions that undo/redo as a single step
    // undo.beginGroup(): tek adim olarak geri alinacak/yinelenecek bir eylem grubu baslat
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "beginGroup"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getUndo().beginGroup();
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // undo.endGroup(): end the current action group
    // undo.endGroup(): mevcut eylem grubunu bitir
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "endGroup"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getUndo().endGroup();
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // undo.inGroup(): check if currently inside an undo group
    // undo.inGroup(): su an bir geri alma grubu icinde olup olmadigini kontrol et
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "inGroup"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), bufs->active().getUndo().inGroup()));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // undo.branch(index): switch to a different branch at the current undo node
    // undo.branch(index): mevcut geri alma dugumunde farkli bir dala gec
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "branch"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            int index = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getUndo().branch(index);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // undo.branchCount(): get the number of branches at the current undo node
    // undo.branchCount(): mevcut geri alma dugumundeki dal sayisini al
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "branchCount"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), bufs->active().getUndo().branchCount()));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // undo.currentBranch(): get the active branch index at the current undo node
    // undo.currentBranch(): mevcut geri alma dugumundeki aktif dal indeksini al
    jsUndo->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "currentBranch"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), bufs->active().getUndo().currentBranch()));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
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
