// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "FileBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "V8Engine.h"
#include "buffers.h"
#include "file.h"
#include <v8.h>
#include <filesystem>
namespace fs = std::filesystem;

// Get the calling JS file's absolute path from V8 stack trace
// V8 stack trace'den cagiran JS dosyasinin mutlak yolunu al
std::string getCallerScriptPath(v8::Isolate* isolate) {
    v8::Local<v8::StackTrace> stack = v8::StackTrace::CurrentStackTrace(isolate, 2);
    if (stack->GetFrameCount() < 2) return "";
    v8::Local<v8::StackFrame> frame = stack->GetFrame(isolate, 1);
    v8::String::Utf8Value scriptName(isolate, frame->GetScriptName());
    return (*scriptName) ? std::string(*scriptName) : "";
}

// Context struct to pass buffers pointer and i18n to lambda callbacks
// Lambda callback'lere hem buffers hem i18n isaretcisini aktarmak icin baglam yapisi
struct FileCtx {
    Buffers* bufs;
    I18n* i18n;
};

// Register file I/O API on editor.file JS object (load, save, loadText, saveText, exists, info, delete, rename)
// editor.file JS nesnesine dosya I/O API'sini kaydet (load, save, loadText, saveText, exists, info, delete, rename)
void RegisterFileBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsFile = v8::Object::New(isolate);

    auto* fctx = new FileCtx{ctx.buffers, ctx.i18n};

    // file.load(path) -> {ok, data: true/false, ...}
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "load"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* fc = static_cast<FileCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc || !fc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, fc ? fc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "path"}}, fc->i18n);
                return;
            }
            v8::String::Utf8Value pathArg(args.GetIsolate(), args[0]);
            std::string path = *pathArg;
            if (!path.empty() && !fs::path(path).is_absolute()) {
                std::string caller = getCallerScriptPath(args.GetIsolate());
                if (!caller.empty()) path = V8Engine::resolveModulePath(path, caller);
            }
            FileResult result = FileSystem::loadToBuffer(fc->bufs->active().getBuffer(), path);
            if (result.success) {
                V8Response::ok(args, true, nullptr, "file.load.success",
                    {{"path", path}}, fc->i18n);
            } else {
                V8Response::error(args, "LOAD_ERROR", "file.load.error",
                    {{"path", path}}, fc->i18n);
            }
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // file.save(path) -> {ok, data: true/false, ...}
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "save"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* fc = static_cast<FileCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc || !fc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, fc ? fc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "path"}}, fc->i18n);
                return;
            }
            v8::String::Utf8Value pathArg(args.GetIsolate(), args[0]);
            std::string path = *pathArg;
            if (!path.empty() && !fs::path(path).is_absolute()) {
                std::string caller = getCallerScriptPath(args.GetIsolate());
                if (!caller.empty()) path = V8Engine::resolveModulePath(path, caller);
            }
            FileResult result = FileSystem::saveFromBuffer(fc->bufs->active().getBuffer(), path);
            if (result.success) {
                V8Response::ok(args, true, nullptr, "file.save.success",
                    {{"path", path}}, fc->i18n);
            } else {
                V8Response::error(args, "SAVE_ERROR", "file.save.error",
                    {{"path", path}}, fc->i18n);
            }
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // file.loadText(path) -> {ok, data: "content"|null, ...}
    // Buffer bagimliligi olmadan metin dosyasi yukle
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "loadText"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* fc = static_cast<FileCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "path"}}, fc->i18n);
                return;
            }
            v8::String::Utf8Value pathArg(args.GetIsolate(), args[0]);
            std::string path = *pathArg;
            if (!path.empty() && !fs::path(path).is_absolute()) {
                std::string caller = getCallerScriptPath(args.GetIsolate());
                if (!caller.empty()) path = V8Engine::resolveModulePath(path, caller);
            }
            auto content = FileSystem::loadTextFile(path);
            if (!content.has_value()) {
                V8Response::error(args, "LOAD_ERROR", "file.loadtext.error",
                    {{"path", path}}, fc->i18n);
                return;
            }
            V8Response::ok(args, *content);
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // file.saveText(path, content) -> {ok, data: true/false, ...}
    // Buffer bagimliligi olmadan metin dosyasi kaydet
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "saveText"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* fc = static_cast<FileCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "path, content"}}, fc->i18n);
                return;
            }
            v8::String::Utf8Value pathArg(args.GetIsolate(), args[0]);
            v8::String::Utf8Value content(args.GetIsolate(), args[1]);
            std::string path = *pathArg;
            if (!path.empty() && !fs::path(path).is_absolute()) {
                std::string caller = getCallerScriptPath(args.GetIsolate());
                if (!caller.empty()) path = V8Engine::resolveModulePath(path, caller);
            }
            bool ok = FileSystem::saveTextFile(path, *content);
            if (ok) {
                V8Response::ok(args, true, nullptr, "file.savetext.success",
                    {{"path", path}}, fc->i18n);
            } else {
                V8Response::error(args, "SAVE_ERROR", "file.savetext.error",
                    {{"path", path}}, fc->i18n);
            }
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // file.exists(path) -> {ok, data: bool, ...}
    // Dosyanin var olup olmadigini kontrol et
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "exists"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* fc = static_cast<FileCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "path"}}, fc->i18n);
                return;
            }
            v8::String::Utf8Value pathArg(args.GetIsolate(), args[0]);
            std::string path = *pathArg;
            if (!path.empty() && !fs::path(path).is_absolute()) {
                std::string caller = getCallerScriptPath(args.GetIsolate());
                if (!caller.empty()) path = V8Engine::resolveModulePath(path, caller);
            }
            bool exists = FileSystem::exists(path);
            V8Response::ok(args, exists);
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // file.isReadable(path) -> {ok, data: bool, ...}
    // Dosyanin okunabilir olup olmadigini kontrol et
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isReadable"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* fc = static_cast<FileCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "path"}}, fc->i18n);
                return;
            }
            v8::String::Utf8Value pathArg(args.GetIsolate(), args[0]);
            std::string path = *pathArg;
            if (!path.empty() && !fs::path(path).is_absolute()) {
                std::string caller = getCallerScriptPath(args.GetIsolate());
                if (!caller.empty()) path = V8Engine::resolveModulePath(path, caller);
            }
            bool readable = FileSystem::isReadable(path);
            V8Response::ok(args, readable);
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // file.isWritable(path) -> {ok, data: bool, ...}
    // Dosyanin yazilabilir olup olmadigini kontrol et
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isWritable"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* fc = static_cast<FileCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "path"}}, fc->i18n);
                return;
            }
            v8::String::Utf8Value pathArg(args.GetIsolate(), args[0]);
            std::string path = *pathArg;
            if (!path.empty() && !fs::path(path).is_absolute()) {
                std::string caller = getCallerScriptPath(args.GetIsolate());
                if (!caller.empty()) path = V8Engine::resolveModulePath(path, caller);
            }
            bool writable = FileSystem::isWritable(path);
            V8Response::ok(args, writable);
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // file.delete(path) -> {ok, data: bool, ...}
    // Dosyayi sil
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "delete"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* fc = static_cast<FileCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "path"}}, fc->i18n);
                return;
            }
            v8::String::Utf8Value pathArg(args.GetIsolate(), args[0]);
            std::string path = *pathArg;
            if (!path.empty() && !fs::path(path).is_absolute()) {
                std::string caller = getCallerScriptPath(args.GetIsolate());
                if (!caller.empty()) path = V8Engine::resolveModulePath(path, caller);
            }
            bool ok = false;
            try {
                ok = fs::remove(path);
            } catch (...) { ok = false; }
            if (ok) {
                V8Response::ok(args, true, nullptr, "file.delete.success",
                    {{"path", path}}, fc->i18n);
            } else {
                V8Response::error(args, "DELETE_ERROR", "file.delete.error",
                    {{"path", path}}, fc->i18n);
            }
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // file.rename(oldPath, newPath) -> {ok, data: bool, ...}
    // Dosyayi yeniden adlandir
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "rename"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* fc = static_cast<FileCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "oldPath, newPath"}}, fc->i18n);
                return;
            }
            v8::String::Utf8Value oldPathArg(args.GetIsolate(), args[0]);
            v8::String::Utf8Value newPathArg(args.GetIsolate(), args[1]);
            std::string oldPath = *oldPathArg;
            std::string newPath = *newPathArg;
            if (!oldPath.empty() && !fs::path(oldPath).is_absolute()) {
                std::string caller = getCallerScriptPath(args.GetIsolate());
                if (!caller.empty()) oldPath = V8Engine::resolveModulePath(oldPath, caller);
            }
            if (!newPath.empty() && !fs::path(newPath).is_absolute()) {
                std::string caller = getCallerScriptPath(args.GetIsolate());
                if (!caller.empty()) newPath = V8Engine::resolveModulePath(newPath, caller);
            }
            bool ok = false;
            try {
                fs::rename(oldPath, newPath);
                ok = true;
            } catch (...) { ok = false; }
            if (ok) {
                V8Response::ok(args, true, nullptr, "file.rename.success",
                    {{"oldPath", oldPath}, {"newPath", newPath}}, fc->i18n);
            } else {
                V8Response::error(args, "RENAME_ERROR", "file.rename.error",
                    {{"oldPath", oldPath}, {"newPath", newPath}}, fc->i18n);
            }
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // file.info(path) -> {ok, data: {path, size}, ...}
    // Dosya bilgilerini al
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "info"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* fc = static_cast<FileCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "path"}}, fc->i18n);
                return;
            }
            v8::String::Utf8Value pathArg(args.GetIsolate(), args[0]);
            std::string path = *pathArg;
            if (!path.empty() && !fs::path(path).is_absolute()) {
                std::string caller = getCallerScriptPath(args.GetIsolate());
                if (!caller.empty()) path = V8Engine::resolveModulePath(path, caller);
            }
            auto info = FileSystem::getFileInfo(path);
            if (!info.has_value()) {
                V8Response::error(args, "INFO_ERROR", "file.info.error",
                    {{"path", path}}, fc->i18n);
                return;
            }
            json data = {
                {"path", info->path},
                {"size", static_cast<double>(info->size)}
            };
            V8Response::ok(args, data);
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // file.copyFile(src, dst) -> {ok, data: bool, ...} - Copy a file from source to destination
    // Dosyayi kaynaktan hedefe kopyala
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "copyFile"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* fc = static_cast<FileCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "src, dst"}}, fc->i18n);
                return;
            }
            v8::String::Utf8Value srcArg(args.GetIsolate(), args[0]);
            v8::String::Utf8Value dstArg(args.GetIsolate(), args[1]);
            std::string src = *srcArg;
            std::string dst = *dstArg;
            if (!src.empty() && !fs::path(src).is_absolute()) {
                std::string caller = getCallerScriptPath(args.GetIsolate());
                if (!caller.empty()) src = V8Engine::resolveModulePath(src, caller);
            }
            if (!dst.empty() && !fs::path(dst).is_absolute()) {
                std::string caller = getCallerScriptPath(args.GetIsolate());
                if (!caller.empty()) dst = V8Engine::resolveModulePath(dst, caller);
            }
            bool ok = FileSystem::copyFile(src, dst);
            if (ok) {
                V8Response::ok(args, true, nullptr, "file.copy.success",
                    {{"src", src}, {"dst", dst}}, fc->i18n);
            } else {
                V8Response::error(args, "COPY_ERROR", "file.copy.error",
                    {{"src", src}, {"dst", dst}}, fc->i18n);
            }
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // file.hasUTF8BOM(path) -> {ok, data: bool, ...} - Check if file starts with UTF-8 BOM
    // Dosyanin UTF-8 BOM ile baslayip baslamadigini kontrol et
    jsFile->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "hasUTF8BOM"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args){
            auto* fc = static_cast<FileCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "path"}}, fc->i18n);
                return;
            }
            v8::String::Utf8Value pathArg(args.GetIsolate(), args[0]);
            std::string path = *pathArg;
            if (!path.empty() && !fs::path(path).is_absolute()) {
                std::string caller = getCallerScriptPath(args.GetIsolate());
                if (!caller.empty()) path = V8Engine::resolveModulePath(path, caller);
            }
            bool hasBom = FileSystem::hasUTF8BOM(path);
            V8Response::ok(args, hasBom);
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
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
