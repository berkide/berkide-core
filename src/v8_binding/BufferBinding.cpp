#include "BufferBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "buffers.h"
#include "file.h"
#include <v8.h>

// Register buffer API on editor.buffer JS object (load, save, getLine, insertChar, deleteChar, etc.)
// editor.buffer JS nesnesine buffer API'sini kaydet (load, save, getLine, insertChar, deleteChar, vb.)
void RegisterBufferBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsBuffer = v8::Object::New(isolate);
    Buffers* buffers = ctx.buffers;

    // buffer.load(path)
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "load"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            auto& buf = bufs->active().getBuffer();
            FileResult res = FileSystem::loadToBuffer(buf, *path);
            v8::Local<v8::Object> result = v8::Object::New(args.GetIsolate());
            result->Set(args.GetIsolate()->GetCurrentContext(),
                v8::String::NewFromUtf8Literal(args.GetIsolate(), "success"),
                v8::Boolean::New(args.GetIsolate(), res.success)).Check();
            result->Set(args.GetIsolate()->GetCurrentContext(),
                v8::String::NewFromUtf8Literal(args.GetIsolate(), "message"),
                v8::String::NewFromUtf8(args.GetIsolate(), res.message.c_str()).ToLocalChecked()).Check();
            args.GetReturnValue().Set(result);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffer.save(path)
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "save"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            auto& buf = bufs->active().getBuffer();
            FileResult res = FileSystem::saveFromBuffer(buf, *path);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), res.success));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffer.getLine(index)
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getLine"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            int line = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            std::string s = bufs->active().getBuffer().getLine(line);
            args.GetReturnValue().Set(v8::String::NewFromUtf8(args.GetIsolate(), s.c_str()).ToLocalChecked());
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffer.insertChar(line, col, char)
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "insertChar"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 3) return;
            int line = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int col  = args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            v8::String::Utf8Value chStr(args.GetIsolate(), args[2]);
            if (chStr.length() == 0) return;
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            // Use insertText for Unicode support
            std::string text(*chStr, chStr.length());
            bufs->active().getBuffer().insertText(line, col, text);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffer.deleteChar(line, col)
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "deleteChar"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 2) return;
            int line = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int col  = args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getBuffer().deleteChar(line, col);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffer.insertLineAt(index, text)
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "insertLineAt"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 2) return;
            int index = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            v8::String::Utf8Value text(args.GetIsolate(), args[1]);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getBuffer().insertLineAt(index, *text);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffer.lineCount()
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "lineCount"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), bufs->active().getBuffer().lineCount()));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffer.deleteLine(index)
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "deleteLine"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            int index = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getBuffer().deleteLine(index);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffer.insertText(line, col, text): insert multi-character text at position
    // buffer.insertText(line, col, text): konuma cok karakterli metin ekle
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "insertText"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 3) return;
            int line = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int col  = args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            v8::String::Utf8Value text(args.GetIsolate(), args[2]);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getBuffer().insertText(line, col, std::string(*text, text.length()));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffer.deleteRange(lineStart, colStart, lineEnd, colEnd): delete text between two positions
    // buffer.deleteRange(lineStart, colStart, lineEnd, colEnd): iki konum arasindaki metni sil
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "deleteRange"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 4) return;
            auto ctx = args.GetIsolate()->GetCurrentContext();
            int lineStart = args[0]->Int32Value(ctx).FromMaybe(0);
            int colStart  = args[1]->Int32Value(ctx).FromMaybe(0);
            int lineEnd   = args[2]->Int32Value(ctx).FromMaybe(0);
            int colEnd    = args[3]->Int32Value(ctx).FromMaybe(0);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getBuffer().deleteRange(lineStart, colStart, lineEnd, colEnd);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffer.splitLine(line, col): split a line into two at given column
    // buffer.splitLine(line, col): verilen sutunda bir satiri ikiye bol
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "splitLine"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 2) return;
            int line = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int col  = args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getBuffer().splitLine(line, col);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffer.joinLines(first, second): join two consecutive lines into one
    // buffer.joinLines(first, second): ardisik iki satiri birlestir
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "joinLines"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 2) return;
            int first  = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int second = args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getBuffer().joinLines(first, second);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffer.columnCount(line): get number of characters in a specific line
    // buffer.columnCount(line): belirli bir satirdaki karakter sayisini al
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "columnCount"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            int line = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), bufs->active().getBuffer().columnCount(line)));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffer.clear(): clear all content, reset to single empty line
    // buffer.clear(): tum icerigi temizle, tek bos satira sifirla
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clear"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getBuffer().clear();
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffer.isValidPos(line, col): check if position is valid within the buffer
    // buffer.isValidPos(line, col): konumun buffer icinde gecerli olup olmadigini kontrol et
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isValidPos"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 2) return;
            int line = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int col  = args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), bufs->active().getBuffer().isValidPos(line, col)));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffer.insertLine(text): append a new line at the end of the buffer
    // buffer.insertLine(text): buffer'in sonuna yeni satir ekle
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "insertLine"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            v8::String::Utf8Value text(args.GetIsolate(), args[0]);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().getBuffer().insertLine(std::string(*text, text.length()));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx, v8::String::NewFromUtf8Literal(isolate, "buffer"), jsBuffer).Check();
}

// Auto-register "buffer" binding at static init time so it is applied when editor object is created
// "buffer" binding'ini statik baslangicta otomatik kaydet, editor nesnesi olusturulurken uygulansin
static bool registered_buffer = []{
    BindingRegistry::instance().registerBinding("buffer",
        [](v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx){
            RegisterBufferBinding(isolate, editorObj, ctx);
        });
    return true;
}();
