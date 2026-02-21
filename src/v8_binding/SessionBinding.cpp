#include "SessionBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "SessionManager.h"
#include "buffers.h"

// Context struct for session binding closures
// Oturum binding kapanislari icin baglam yapisi
struct SessionBindCtx {
    SessionManager* session;
    Buffers* buffers;
};

// Register editor.session JS API
// editor.session JS API'sini kaydet
void RegisterSessionBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsSession = v8::Object::New(isolate);

    auto* sc = new SessionBindCtx{ctx.sessionManager, ctx.buffers};
    auto ext = v8::External::New(isolate, sc);

    // editor.session.save() -> bool
    // Mevcut oturumu kaydet
    jsSession->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "save"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<SessionBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!c->session || !c->buffers) { args.GetReturnValue().Set(false); return; }
            bool ok = c->session->save(*c->buffers);
            args.GetReturnValue().Set(ok);
        }, ext).ToLocalChecked()
    ).Check();

    // editor.session.load() -> {documents: [...], activeIndex, workingDir} | null
    // Oturumu yukle
    jsSession->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "load"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<SessionBindCtx*>(args.Data().As<v8::External>()->Value());
            auto iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            if (!c->session) { args.GetReturnValue().SetNull(); return; }

            SessionState state;
            if (!c->session->load(state)) {
                args.GetReturnValue().SetNull();
                return;
            }

            v8::Local<v8::Object> result = v8::Object::New(iso);
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "activeIndex"),
                        v8::Integer::New(iso, state.activeIndex)).Check();
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "workingDir"),
                        v8::String::NewFromUtf8(iso, state.lastWorkingDir.c_str()).ToLocalChecked()).Check();

            v8::Local<v8::Array> docs = v8::Array::New(iso, static_cast<int>(state.documents.size()));
            for (size_t i = 0; i < state.documents.size(); ++i) {
                v8::Local<v8::Object> doc = v8::Object::New(iso);
                doc->Set(ctx, v8::String::NewFromUtf8Literal(iso, "filePath"),
                         v8::String::NewFromUtf8(iso, state.documents[i].filePath.c_str()).ToLocalChecked()).Check();
                doc->Set(ctx, v8::String::NewFromUtf8Literal(iso, "cursorLine"),
                         v8::Integer::New(iso, state.documents[i].cursorLine)).Check();
                doc->Set(ctx, v8::String::NewFromUtf8Literal(iso, "cursorCol"),
                         v8::Integer::New(iso, state.documents[i].cursorCol)).Check();
                doc->Set(ctx, v8::String::NewFromUtf8Literal(iso, "scrollTop"),
                         v8::Integer::New(iso, state.documents[i].scrollTop)).Check();
                docs->Set(ctx, static_cast<uint32_t>(i), doc).Check();
            }
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "documents"), docs).Check();

            args.GetReturnValue().Set(result);
        }, ext).ToLocalChecked()
    ).Check();

    // editor.session.saveAs(name) -> bool
    // Oturumu adla kaydet
    jsSession->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "saveAs"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<SessionBindCtx*>(args.Data().As<v8::External>()->Value());
            if (args.Length() < 1 || !args[0]->IsString()) { args.GetReturnValue().Set(false); return; }
            v8::String::Utf8Value name(args.GetIsolate(), args[0]);
            bool ok = c->session->saveAs(*name, *c->buffers);
            args.GetReturnValue().Set(ok);
        }, ext).ToLocalChecked()
    ).Check();

    // editor.session.loadFrom(name) -> object | null
    // Adlandirilmis oturumu yukle
    jsSession->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "loadFrom"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<SessionBindCtx*>(args.Data().As<v8::External>()->Value());
            auto iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            if (args.Length() < 1 || !args[0]->IsString()) { args.GetReturnValue().SetNull(); return; }
            v8::String::Utf8Value name(iso, args[0]);

            SessionState state;
            if (!c->session->loadFrom(*name, state)) {
                args.GetReturnValue().SetNull();
                return;
            }

            v8::Local<v8::Object> result = v8::Object::New(iso);
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "activeIndex"),
                        v8::Integer::New(iso, state.activeIndex)).Check();

            v8::Local<v8::Array> docs = v8::Array::New(iso, static_cast<int>(state.documents.size()));
            for (size_t i = 0; i < state.documents.size(); ++i) {
                v8::Local<v8::Object> doc = v8::Object::New(iso);
                doc->Set(ctx, v8::String::NewFromUtf8Literal(iso, "filePath"),
                         v8::String::NewFromUtf8(iso, state.documents[i].filePath.c_str()).ToLocalChecked()).Check();
                doc->Set(ctx, v8::String::NewFromUtf8Literal(iso, "cursorLine"),
                         v8::Integer::New(iso, state.documents[i].cursorLine)).Check();
                doc->Set(ctx, v8::String::NewFromUtf8Literal(iso, "cursorCol"),
                         v8::Integer::New(iso, state.documents[i].cursorCol)).Check();
                docs->Set(ctx, static_cast<uint32_t>(i), doc).Check();
            }
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "documents"), docs).Check();
            args.GetReturnValue().Set(result);
        }, ext).ToLocalChecked()
    ).Check();

    // editor.session.list() -> [string]
    // Kaydedilmis oturumlari listele
    jsSession->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<SessionBindCtx*>(args.Data().As<v8::External>()->Value());
            auto iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            auto names = c->session->listSessions();
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(names.size()));
            for (size_t i = 0; i < names.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i),
                         v8::String::NewFromUtf8(iso, names[i].c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(arr);
        }, ext).ToLocalChecked()
    ).Check();

    // editor.session.remove(name) -> bool
    // Adlandirilmis oturumu sil
    jsSession->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "remove"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<SessionBindCtx*>(args.Data().As<v8::External>()->Value());
            if (args.Length() < 1 || !args[0]->IsString()) { args.GetReturnValue().Set(false); return; }
            v8::String::Utf8Value name(args.GetIsolate(), args[0]);
            bool ok = c->session->deleteSession(*name);
            args.GetReturnValue().Set(ok);
        }, ext).ToLocalChecked()
    ).Check();

    // editor.session.setSessionPath(path) - Set the session file path
    // Oturum dosyasi yolunu ayarla
    jsSession->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setSessionPath"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<SessionBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!c->session || args.Length() < 1 || !args[0]->IsString()) return;
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            c->session->setSessionPath(*path);
        }, ext).ToLocalChecked()
    ).Check();

    // editor.session.lastState() -> {documents, activeIndex, workingDir, windowWidth, windowHeight}
    // Son kaydedilen oturum durumunu al
    jsSession->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "lastState"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<SessionBindCtx*>(args.Data().As<v8::External>()->Value());
            auto iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            if (!c->session) { args.GetReturnValue().SetNull(); return; }

            const SessionState& state = c->session->lastState();

            v8::Local<v8::Object> result = v8::Object::New(iso);
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "activeIndex"),
                        v8::Integer::New(iso, state.activeIndex)).Check();
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "workingDir"),
                        v8::String::NewFromUtf8(iso, state.lastWorkingDir.c_str()).ToLocalChecked()).Check();
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "windowWidth"),
                        v8::Integer::New(iso, state.windowWidth)).Check();
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "windowHeight"),
                        v8::Integer::New(iso, state.windowHeight)).Check();

            v8::Local<v8::Array> docs = v8::Array::New(iso, static_cast<int>(state.documents.size()));
            for (size_t i = 0; i < state.documents.size(); ++i) {
                v8::Local<v8::Object> doc = v8::Object::New(iso);
                doc->Set(ctx, v8::String::NewFromUtf8Literal(iso, "filePath"),
                         v8::String::NewFromUtf8(iso, state.documents[i].filePath.c_str()).ToLocalChecked()).Check();
                doc->Set(ctx, v8::String::NewFromUtf8Literal(iso, "cursorLine"),
                         v8::Integer::New(iso, state.documents[i].cursorLine)).Check();
                doc->Set(ctx, v8::String::NewFromUtf8Literal(iso, "cursorCol"),
                         v8::Integer::New(iso, state.documents[i].cursorCol)).Check();
                doc->Set(ctx, v8::String::NewFromUtf8Literal(iso, "scrollTop"),
                         v8::Integer::New(iso, state.documents[i].scrollTop)).Check();
                doc->Set(ctx, v8::String::NewFromUtf8Literal(iso, "isActive"),
                         v8::Boolean::New(iso, state.documents[i].isActive)).Check();
                docs->Set(ctx, static_cast<uint32_t>(i), doc).Check();
            }
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "documents"), docs).Check();

            args.GetReturnValue().Set(result);
        }, ext).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "session"), jsSession).Check();
}

// Auto-register binding
// Binding'i otomatik kaydet
static bool _sessionReg = [] {
    BindingRegistry::instance().registerBinding("session", RegisterSessionBinding);
    return true;
}();
