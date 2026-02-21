#include "CursorBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "buffers.h"
#include <v8.h>

// Register cursor API on editor.cursor JS object (getLine, getCol, setPosition, moveUp/Down/Left/Right, etc.)
// editor.cursor JS nesnesine cursor API'sini kaydet (getLine, getCol, setPosition, moveUp/Down/Left/Right, vb.)
void RegisterCursorBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsCursor = v8::Object::New(isolate);
    Buffers* buffers = ctx.buffers;

    // cursor.getLine()
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getLine"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), bufs->active().getCursor().getLine()));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // cursor.getCol()
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getCol"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), bufs->active().getCursor().getCol()));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // cursor.setPosition(line, col)
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setPosition"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 2) return;
            int line = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int col  = args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getCursor().setPosition(line, col);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // cursor.moveUp()
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveUp"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getCursor().moveUp(bufs->active().getBuffer());
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // cursor.moveDown()
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveDown"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getCursor().moveDown(bufs->active().getBuffer());
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // cursor.moveLeft()
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveLeft"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getCursor().moveLeft(bufs->active().getBuffer());
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // cursor.moveRight()
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveRight"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getCursor().moveRight(bufs->active().getBuffer());
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // cursor.moveToLineEnd()
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveToLineEnd"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getCursor().moveToLineEnd(bufs->active().getBuffer());
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // cursor.clampToBuffer()
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clampToBuffer"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getCursor().clampToBuffer(bufs->active().getBuffer());
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // cursor.moveToLineStart(): move cursor to the beginning of the current line
    // cursor.moveToLineStart(): imleci mevcut satirin basina tasi
    jsCursor->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveToLineStart"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            auto& cursor = bufs->active().getCursor();
            cursor.setPosition(cursor.getLine(), 0);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
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
