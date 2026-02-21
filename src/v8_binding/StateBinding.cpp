#include "StateBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "buffers.h"
#include <v8.h>

// Register state API on editor.state JS object (getMode, setMode, isModified, filePath, markModified, reset)
// editor.state JS nesnesine state API'sini kaydet (getMode, setMode, isModified, filePath, markModified, reset)
void RegisterStateBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx)
{
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsState = v8::Object::New(isolate);
    Buffers* buffers = ctx.buffers;

    // state.getMode()
    jsState->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getMode"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            EditMode mode = bufs->active().getMode();
            const char* m = "normal";
            if (mode == EditMode::Insert) m = "insert";
            else if (mode == EditMode::Visual) m = "visual";
            args.GetReturnValue().Set(v8::String::NewFromUtf8(args.GetIsolate(), m).ToLocalChecked());
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // state.setMode(modeStr)
    jsState->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setMode"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            v8::String::Utf8Value s(args.GetIsolate(), args[0]);
            EditMode m = EditMode::Normal;
            if (std::string(*s) == "insert") m = EditMode::Insert;
            else if (std::string(*s) == "visual") m = EditMode::Visual;
            bufs->active().setMode(m);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // state.isModified()
    jsState->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isModified"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), bufs->active().isModified()));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // state.filePath()
    jsState->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "filePath"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            auto& p = bufs->active().getFilePath();
            args.GetReturnValue().Set(v8::String::NewFromUtf8(args.GetIsolate(), p.c_str()).ToLocalChecked());
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // state.markModified(bool)
    jsState->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "markModified"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bool b = (args.Length() > 0) ? args[0]->BooleanValue(args.GetIsolate()) : true;
            bufs->active().markModified(b);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // state.reset()
    jsState->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "reset"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            bufs->active().reset();
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // state.setFilePath(path) - Set the file path for current document
    // Mevcut belge icin dosya yolunu ayarla
    jsState->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setFilePath"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            bufs->active().setFilePath(*path);
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
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
