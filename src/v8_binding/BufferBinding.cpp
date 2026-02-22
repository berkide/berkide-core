// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "BufferBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "buffers.h"
#include "file.h"
#include <v8.h>

// Context struct to pass both buffers pointer and i18n to lambda callbacks
// Lambda callback'lere hem buffers hem i18n isaretcisini aktarmak icin baglam yapisi
struct BufferCtx {
    Buffers* bufs;
    I18n* i18n;
};

// Register buffer API on editor.buffer JS object (load, save, getLine, insertChar, deleteChar, etc.)
// editor.buffer JS nesnesine buffer API'sini kaydet (load, save, getLine, insertChar, deleteChar, vb.)
void RegisterBufferBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsBuffer = v8::Object::New(isolate);

    auto* bctx = new BufferCtx{ctx.buffers, ctx.i18n};

    // buffer.load(path) -> {ok, data: {success, message}, ...}
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "load"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bc = static_cast<BufferCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "path"}}, bc->i18n);
                return;
            }
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            auto& buf = bc->bufs->active().getBuffer();
            FileResult res = FileSystem::loadToBuffer(buf, *path);
            if (res.success) {
                json data = {{"success", true}, {"message", res.message}};
                V8Response::ok(args, data, nullptr, "buffer.load.success",
                    {{"path", *path}}, bc->i18n);
            } else {
                V8Response::error(args, "LOAD_ERROR", "buffer.load.error",
                    {{"path", *path}}, bc->i18n);
            }
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffer.save(path) -> {ok, data: true/false, ...}
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "save"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bc = static_cast<BufferCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "path"}}, bc->i18n);
                return;
            }
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            auto& buf = bc->bufs->active().getBuffer();
            FileResult res = FileSystem::saveFromBuffer(buf, *path);
            if (res.success) {
                V8Response::ok(args, true, nullptr, "buffer.save.success",
                    {{"path", *path}}, bc->i18n);
            } else {
                V8Response::error(args, "SAVE_ERROR", "buffer.save.error",
                    {{"path", *path}}, bc->i18n);
            }
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffer.getLine(index) -> {ok, data: "line content", ...}
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getLine"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bc = static_cast<BufferCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "index"}}, bc->i18n);
                return;
            }
            int line = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            auto& buf = bc->bufs->active().getBuffer();
            if (line < 0 || line >= buf.lineCount()) {
                V8Response::error(args, "INVALID_LINE", "buffer.getline.invalid",
                    {{"line", std::to_string(line)}}, bc->i18n);
                return;
            }
            std::string s = buf.getLine(line);
            V8Response::ok(args, s);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffer.insertChar(line, col, char) -> {ok, data: true, ...}
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "insertChar"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bc = static_cast<BufferCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 3) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "line, col, char"}}, bc->i18n);
                return;
            }
            int line = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int col  = args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            v8::String::Utf8Value chStr(args.GetIsolate(), args[2]);
            if (chStr.length() == 0) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "char"}}, bc->i18n);
                return;
            }
            std::string text(*chStr, chStr.length());
            bc->bufs->active().getBuffer().insertText(line, col, text);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffer.deleteChar(line, col) -> {ok, data: true, ...}
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "deleteChar"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bc = static_cast<BufferCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "line, col"}}, bc->i18n);
                return;
            }
            int line = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int col  = args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            bc->bufs->active().getBuffer().deleteChar(line, col);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffer.insertLineAt(index, text) -> {ok, data: true, ...}
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "insertLineAt"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bc = static_cast<BufferCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "index, text"}}, bc->i18n);
                return;
            }
            int index = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            v8::String::Utf8Value text(args.GetIsolate(), args[1]);
            bc->bufs->active().getBuffer().insertLineAt(index, *text);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffer.lineCount() -> {ok, data: number, meta: {total: number}, ...}
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "lineCount"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bc = static_cast<BufferCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, bc ? bc->i18n : nullptr);
                return;
            }
            int count = bc->bufs->active().getBuffer().lineCount();
            json meta = {{"total", count}};
            V8Response::ok(args, count, meta, "buffer.linecount.success",
                {{"count", std::to_string(count)}}, bc->i18n);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffer.deleteLine(index) -> {ok, data: true, ...}
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "deleteLine"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bc = static_cast<BufferCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "index"}}, bc->i18n);
                return;
            }
            int index = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            bc->bufs->active().getBuffer().deleteLine(index);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffer.insertText(line, col, text) -> {ok, data: true, ...}
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "insertText"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bc = static_cast<BufferCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 3) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "line, col, text"}}, bc->i18n);
                return;
            }
            int line = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int col  = args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            v8::String::Utf8Value text(args.GetIsolate(), args[2]);
            bc->bufs->active().getBuffer().insertText(line, col, std::string(*text, text.length()));
            V8Response::ok(args, true);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffer.deleteRange(lineStart, colStart, lineEnd, colEnd) -> {ok, data: true, ...}
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "deleteRange"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bc = static_cast<BufferCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 4) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "lineStart, colStart, lineEnd, colEnd"}}, bc->i18n);
                return;
            }
            auto ctx = args.GetIsolate()->GetCurrentContext();
            int lineStart = args[0]->Int32Value(ctx).FromMaybe(0);
            int colStart  = args[1]->Int32Value(ctx).FromMaybe(0);
            int lineEnd   = args[2]->Int32Value(ctx).FromMaybe(0);
            int colEnd    = args[3]->Int32Value(ctx).FromMaybe(0);
            bc->bufs->active().getBuffer().deleteRange(lineStart, colStart, lineEnd, colEnd);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffer.splitLine(line, col) -> {ok, data: true, ...}
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "splitLine"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bc = static_cast<BufferCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "line, col"}}, bc->i18n);
                return;
            }
            int line = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int col  = args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            bc->bufs->active().getBuffer().splitLine(line, col);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffer.joinLines(first, second) -> {ok, data: true, ...}
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "joinLines"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bc = static_cast<BufferCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "first, second"}}, bc->i18n);
                return;
            }
            int first  = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int second = args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            bc->bufs->active().getBuffer().joinLines(first, second);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffer.columnCount(line) -> {ok, data: number, ...}
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "columnCount"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bc = static_cast<BufferCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "line"}}, bc->i18n);
                return;
            }
            int line = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int count = bc->bufs->active().getBuffer().columnCount(line);
            V8Response::ok(args, count);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffer.clear() -> {ok, data: true, ...}
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clear"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bc = static_cast<BufferCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, bc ? bc->i18n : nullptr);
                return;
            }
            bc->bufs->active().getBuffer().clear();
            V8Response::ok(args, true, nullptr, "buffer.clear.success", {}, bc->i18n);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffer.isValidPos(line, col) -> {ok, data: bool, ...}
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isValidPos"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bc = static_cast<BufferCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "line, col"}}, bc->i18n);
                return;
            }
            int line = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            int col  = args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            bool valid = bc->bufs->active().getBuffer().isValidPos(line, col);
            V8Response::ok(args, valid);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffer.insertLine(text) -> {ok, data: true, ...}
    jsBuffer->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "insertLine"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bc = static_cast<BufferCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "text"}}, bc->i18n);
                return;
            }
            v8::String::Utf8Value text(args.GetIsolate(), args[0]);
            bc->bufs->active().getBuffer().insertLine(std::string(*text, text.length()));
            V8Response::ok(args, true);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
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
