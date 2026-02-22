// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "BuffersBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "buffers.h"
#include "state.h"
#include <v8.h>

// Context struct for buffers binding lambdas
// Buffer'lar binding lambda'lari icin baglam yapisi
struct BuffersCtx {
    Buffers* bufs;
    I18n* i18n;
};

// Register multi-buffer management API on editor.buffers JS object (newDocument, openFile, saveActive, closeActive, next, prev, etc.)
// editor.buffers JS nesnesine coklu buffer yonetim API'sini kaydet (newDocument, openFile, saveActive, closeActive, next, prev, vb.)
void RegisterBuffersBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsBuffers = v8::Object::New(isolate);

    auto* bctx = new BuffersCtx{ctx.buffers, ctx.i18n};

    // buffers.newDocument(name) -> {ok, data: index, ...}
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "newDocument"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bc = static_cast<BuffersCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, bc ? bc->i18n : nullptr);
                return;
            }
            std::string name = "untitled";
            if (args.Length() > 0) {
                v8::String::Utf8Value n(args.GetIsolate(), args[0]);
                name = *n ? *n : "untitled";
            }
            size_t idx = bc->bufs->newDocument(name);
            V8Response::ok(args, (int)idx);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffers.openFile(path) -> {ok, data: bool, ...}
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "openFile"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bc = static_cast<BuffersCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "path"}}, bc->i18n);
                return;
            }
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            bool result = bc->bufs->openFile(*path);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffers.saveActive() -> {ok, data: bool, ...}
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "saveActive"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bc = static_cast<BuffersCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, bc ? bc->i18n : nullptr);
                return;
            }
            bool result = bc->bufs->saveActive();
            V8Response::ok(args, result);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffers.saveAll() -> {ok, data: savedCount, ...}
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "saveAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bc = static_cast<BuffersCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, bc ? bc->i18n : nullptr);
                return;
            }
            int saved = bc->bufs->saveAll();
            V8Response::ok(args, saved);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffers.closeActive() -> {ok, data: bool, ...}
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "closeActive"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bc = static_cast<BuffersCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, bc ? bc->i18n : nullptr);
                return;
            }
            bool result = bc->bufs->closeActive();
            V8Response::ok(args, result);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffers.count() -> {ok, data: number, ...}
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "count"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bc = static_cast<BuffersCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, bc ? bc->i18n : nullptr);
                return;
            }
            int cnt = (int)bc->bufs->count();
            V8Response::ok(args, cnt);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffers.activeIndex() -> {ok, data: number, ...}
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "activeIndex"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bc = static_cast<BuffersCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, bc ? bc->i18n : nullptr);
                return;
            }
            int idx = (int)bc->bufs->activeIndex();
            V8Response::ok(args, idx);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffers.titleOf(index) -> {ok, data: string, ...}
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "titleOf"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bc = static_cast<BuffersCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "index"}}, bc->i18n);
                return;
            }
            size_t index = args[0]->Uint32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            std::string title = bc->bufs->titleOf(index);
            V8Response::ok(args, title);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffers.next() -> {ok, data: bool, ...}
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "next"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bc = static_cast<BuffersCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, bc ? bc->i18n : nullptr);
                return;
            }
            bool result = bc->bufs->next();
            V8Response::ok(args, result);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffers.prev() -> {ok, data: bool, ...}
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "prev"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bc = static_cast<BuffersCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, bc ? bc->i18n : nullptr);
                return;
            }
            bool result = bc->bufs->prev();
            V8Response::ok(args, result);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffers.closeAt(index): close a buffer at a specific index
    // buffers.closeAt(index): belirli bir indeksteki buffer'i kapat
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "closeAt"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bc = static_cast<BuffersCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "index"}}, bc->i18n);
                return;
            }
            size_t index = args[0]->Uint32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            bool result = bc->bufs->closeAt(index);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffers.setActive(index): switch to a buffer at a specific index
    // buffers.setActive(index): belirli bir indeksteki buffer'a gec
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setActive"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bc = static_cast<BuffersCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "index"}}, bc->i18n);
                return;
            }
            size_t index = args[0]->Uint32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            bool result = bc->bufs->setActive(index);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffers.findByPath(path): find a buffer by file path, returns index or -1
    // buffers.findByPath(path): dosya yoluna gore buffer bul, indeks veya -1 dondur
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "findByPath"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bc = static_cast<BuffersCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "path"}}, bc->i18n);
                return;
            }
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            auto result = bc->bufs->findByPath(*path);
            if (result.has_value()) {
                V8Response::ok(args, (int)result.value());
            } else {
                V8Response::ok(args, -1);
            }
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    // buffers.getStateAt(index): get state info of a buffer at index {filePath, modified, mode}
    // buffers.getStateAt(index): indeksteki buffer'in durum bilgisini al {filePath, modified, mode}
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getStateAt"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bc = static_cast<BuffersCtx*>(args.Data().As<v8::External>()->Value());
            if (!bc || !bc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, bc ? bc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "index"}}, bc->i18n);
                return;
            }
            size_t index = args[0]->Uint32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            if (index >= bc->bufs->count()) {
                V8Response::error(args, "INDEX_OUT_OF_RANGE", "args.index_out_of_range",
                    {{"index", std::to_string(index)}}, bc->i18n);
                return;
            }
            const EditorState& st = bc->bufs->getStateAt(index);
            // Convert EditMode enum to string
            // EditMode enum'unu stringe donustur
            std::string modeStr = "normal";
            if (st.getMode() == EditMode::Insert) modeStr = "insert";
            else if (st.getMode() == EditMode::Visual) modeStr = "visual";

            json data = {
                {"filePath", st.getFilePath()},
                {"modified", st.isModified()},
                {"mode", modeStr}
            };
            V8Response::ok(args, data);
        }, v8::External::New(isolate, bctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx, v8::String::NewFromUtf8Literal(isolate, "buffers"), jsBuffers).Check();
}

// Auto-register "buffers" binding at static init time so it is applied when editor object is created
// "buffers" binding'ini statik baslangicta otomatik kaydet, editor nesnesi olusturulurken uygulansin
static bool registered_buffers = []{
    BindingRegistry::instance().registerBinding("buffers",
        [](v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx){
            RegisterBuffersBinding(isolate, editorObj, ctx);
        });
    return true;
}();
