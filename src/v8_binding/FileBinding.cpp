#include "FileBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "buffers.h"
#include "file.h"
#include <v8.h>
#include <filesystem>
namespace fs = std::filesystem;

// Register file I/O API on editor.file JS object (load, save, loadText, saveText, exists, info, delete, rename)
// editor.file JS nesnesine dosya I/O API'sini kaydet (load, save, loadText, saveText, exists, info, delete, rename)
void RegisterFileBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsFile = v8::Object::New(isolate);
    Buffers* buffers = ctx.buffers;

    // file.load(path)
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "load"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            FileResult result = FileSystem::loadToBuffer(bufs->active().getBuffer(), *path);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), result.success));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // file.save(path)
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "save"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            FileResult result = FileSystem::saveFromBuffer(bufs->active().getBuffer(), *path);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), result.success));
        }, v8::External::New(isolate, buffers)).ToLocalChecked()
    ).Check();

    // file.loadText(path) -- no buffer dependency
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "loadText"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            auto content = FileSystem::loadTextFile(*path);
            if (!content.has_value()) {
                args.GetReturnValue().Set(v8::Null(args.GetIsolate()));
                return;
            }
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(args.GetIsolate(), content->c_str()).ToLocalChecked()
            );
        }).ToLocalChecked()
    ).Check();

    // file.saveText(path, content) -- no buffer dependency
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "saveText"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 2) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            v8::String::Utf8Value content(args.GetIsolate(), args[1]);
            bool ok = FileSystem::saveTextFile(*path, *content);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }).ToLocalChecked()
    ).Check();

    // file.exists(path)
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "exists"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            bool ok = FileSystem::exists(*path);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }).ToLocalChecked()
    ).Check();

    // file.isReadable(path)
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isReadable"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            bool ok = FileSystem::isReadable(*path);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }).ToLocalChecked()
    ).Check();

    // file.isWritable(path)
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isWritable"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            bool ok = FileSystem::isWritable(*path);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }).ToLocalChecked()
    ).Check();

    // file.delete(path)
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "delete"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            bool ok = false;
            try {
                ok = fs::remove(*path);
            } catch (...) { ok = false; }
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }).ToLocalChecked()
    ).Check();

    // file.rename(oldPath, newPath)
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "rename"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 2) return;
            v8::String::Utf8Value oldPath(args.GetIsolate(), args[0]);
            v8::String::Utf8Value newPath(args.GetIsolate(), args[1]);
            bool ok = false;
            try {
                fs::rename(*oldPath, *newPath);
                ok = true;
            } catch (...) { ok = false; }
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }).ToLocalChecked()
    ).Check();

    // file.info(path)
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "info"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            auto info = FileSystem::getFileInfo(*path);
            if (!info.has_value()) {
                args.GetReturnValue().Set(v8::Null(args.GetIsolate()));
                return;
            }
            auto obj = v8::Object::New(args.GetIsolate());
            obj->Set(args.GetIsolate()->GetCurrentContext(),
                v8::String::NewFromUtf8Literal(args.GetIsolate(), "path"),
                v8::String::NewFromUtf8(args.GetIsolate(), info->path.c_str()).ToLocalChecked()).Check();
            obj->Set(args.GetIsolate()->GetCurrentContext(),
                v8::String::NewFromUtf8Literal(args.GetIsolate(), "size"),
                v8::Number::New(args.GetIsolate(), static_cast<double>(info->size))).Check();
            args.GetReturnValue().Set(obj);
        }).ToLocalChecked()
    ).Check();

    // file.copyFile(src, dst) -> bool - Copy a file from source to destination
    // Dosyayi kaynaktan hedefe kopyala
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "copyFile"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 2) return;
            v8::String::Utf8Value src(args.GetIsolate(), args[0]);
            v8::String::Utf8Value dst(args.GetIsolate(), args[1]);
            bool ok = FileSystem::copyFile(*src, *dst);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }).ToLocalChecked()
    ).Check();

    // file.hasUTF8BOM(path) -> bool - Check if file starts with UTF-8 BOM
    // Dosyanin UTF-8 BOM ile baslayip baslamadigini kontrol et
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "hasUTF8BOM"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            if (args.Length() < 1) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            bool ok = FileSystem::hasUTF8BOM(*path);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), ok));
        }).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx, v8::String::NewFromUtf8Literal(isolate, "file"), jsFile).Check();
}

// Auto-register "file" binding at static init time so it is applied when editor object is created
// "file" binding'ini statik baslangicta otomatik kaydet, editor nesnesi olusturulurken uygulansin
static bool registered_file = []{
    BindingRegistry::instance().registerBinding("file",
        [](v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx){
            RegisterFileBinding(isolate, editorObj, ctx);
        });
    return true;
}();
