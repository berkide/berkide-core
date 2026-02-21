#include "BuffersBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "buffers.h"
#include "state.h"
#include <v8.h>

// Register multi-buffer management API on editor.buffers JS object (newDocument, openFile, saveActive, closeActive, next, prev, etc.)
// editor.buffers JS nesnesine coklu buffer yonetim API'sini kaydet (newDocument, openFile, saveActive, closeActive, next, prev, vb.)
void RegisterBuffersBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsBuffers = v8::Object::New(isolate);
    Buffers* buffers = ctx.buffers;

    // buffers.newDocument(name)
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "newDocument"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            std::string name = "untitled";
            if (args.Length() > 0) {
                v8::String::Utf8Value n(args.GetIsolate(), args[0]);
                name = *n;
            }
            auto* b = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            size_t idx = b->newDocument(name);
            args.GetReturnValue().Set((int)idx);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffers.openFile(path)
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "openFile"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            auto* b = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bool ok = b->openFile(*path);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffers.saveActive()
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "saveActive"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* b = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bool ok = b->saveActive();
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffers.saveAll()
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "saveAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* b = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            int saved = b->saveAll();
            args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), saved));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffers.closeActive()
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "closeActive"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* b = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bool ok = b->closeActive();
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffers.count()
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "count"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* b = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            size_t cnt = b->count();
            args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), (int)cnt));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffers.activeIndex()
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "activeIndex"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* b = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            size_t idx = b->activeIndex();
            args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), (int)idx));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffers.titleOf(index)
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "titleOf"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            size_t index = args[0]->Uint32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            auto* b = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            std::string title = b->titleOf(index);
            args.GetReturnValue().Set(v8::String::NewFromUtf8(args.GetIsolate(), title.c_str()).ToLocalChecked());
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffers.next()
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "next"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* b = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bool ok = b->next();
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffers.prev()
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "prev"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* b = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bool ok = b->prev();
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffers.closeAt(index): close a buffer at a specific index
    // buffers.closeAt(index): belirli bir indeksteki buffer'i kapat
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "closeAt"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            size_t index = args[0]->Uint32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            auto* b = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bool ok = b->closeAt(index);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffers.setActive(index): switch to a buffer at a specific index
    // buffers.setActive(index): belirli bir indeksteki buffer'a gec
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setActive"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            size_t index = args[0]->Uint32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            auto* b = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bool ok = b->setActive(index);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffers.findByPath(path): find a buffer by file path, returns index or -1
    // buffers.findByPath(path): dosya yoluna gore buffer bul, indeks veya -1 dondur
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "findByPath"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            auto* b = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            auto result = b->findByPath(*path);
            if (result.has_value()) {
                args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), (int)result.value()));
            } else {
                args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), -1));
            }
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // buffers.getStateAt(index): get state info of a buffer at index {filePath, modified, mode}
    // buffers.getStateAt(index): indeksteki buffer'in durum bilgisini al {filePath, modified, mode}
    jsBuffers->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getStateAt"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            size_t index = args[0]->Uint32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(0);
            auto* b = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            if (index >= b->count()) return;
            const EditorState& st = b->getStateAt(index);
            v8::Isolate* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            v8::Local<v8::Object> obj = v8::Object::New(iso);
            obj->Set(ctx,
                v8::String::NewFromUtf8Literal(iso, "filePath"),
                v8::String::NewFromUtf8(iso, st.getFilePath().c_str()).ToLocalChecked()).Check();
            obj->Set(ctx,
                v8::String::NewFromUtf8Literal(iso, "modified"),
                v8::Boolean::New(iso, st.isModified())).Check();
            // Convert EditMode enum to string
            // EditMode enum'unu stringe donustur
            const char* modeStr = "normal";
            if (st.getMode() == EditMode::Insert) modeStr = "insert";
            else if (st.getMode() == EditMode::Visual) modeStr = "visual";
            obj->Set(ctx,
                v8::String::NewFromUtf8Literal(iso, "mode"),
                v8::String::NewFromUtf8(iso, modeStr).ToLocalChecked()).Check();
            args.GetReturnValue().Set(obj);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
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
