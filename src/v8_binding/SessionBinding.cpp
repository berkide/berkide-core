// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "SessionBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "SessionManager.h"
#include "buffers.h"

// Helper: convert SessionDocument to json
// Yardimci: SessionDocument'i json'a cevir
static json sessionDocToJson(const SessionDocument& doc, bool includeScrollTop = true, bool includeIsActive = false) {
    json obj = {
        {"filePath", doc.filePath},
        {"cursorLine", doc.cursorLine},
        {"cursorCol", doc.cursorCol}
    };
    if (includeScrollTop) {
        obj["scrollTop"] = doc.scrollTop;
    }
    if (includeIsActive) {
        obj["isActive"] = doc.isActive;
    }
    return obj;
}

// Context struct for session binding closures
// Oturum binding kapanislari icin baglam yapisi
struct SessionBindCtx {
    SessionManager* session;
    Buffers* buffers;
    I18n* i18n;
};

// Register editor.session JS API
// editor.session JS API'sini kaydet
void RegisterSessionBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsSession = v8::Object::New(isolate);

    auto* sc = new SessionBindCtx{ctx.sessionManager, ctx.buffers, ctx.i18n};
    auto ext = v8::External::New(isolate, sc);

    // editor.session.save() -> {ok, data: bool, ...}
    // Mevcut oturumu kaydet
    jsSession->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "save"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<SessionBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!c || !c->session || !c->buffers) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "sessionManager"}}, c ? c->i18n : nullptr);
                return;
            }
            bool result = c->session->save(*c->buffers);
            V8Response::ok(args, result);
        }, ext).ToLocalChecked()
    ).Check();

    // editor.session.load() -> {ok, data: {documents, activeIndex, workingDir} | null, ...}
    // Oturumu yukle
    jsSession->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "load"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<SessionBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!c || !c->session) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "sessionManager"}}, c ? c->i18n : nullptr);
                return;
            }

            SessionState state;
            if (!c->session->load(state)) {
                V8Response::ok(args, nullptr);
                return;
            }

            json docs = json::array();
            for (size_t i = 0; i < state.documents.size(); ++i) {
                docs.push_back(sessionDocToJson(state.documents[i]));
            }

            json data = {
                {"activeIndex", state.activeIndex},
                {"workingDir", state.lastWorkingDir},
                {"documents", docs}
            };
            V8Response::ok(args, data);
        }, ext).ToLocalChecked()
    ).Check();

    // editor.session.saveAs(name) -> {ok, data: bool, ...}
    // Oturumu adla kaydet
    jsSession->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "saveAs"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<SessionBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!c || !c->session || !c->buffers) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "sessionManager"}}, c ? c->i18n : nullptr);
                return;
            }
            if (args.Length() < 1 || !args[0]->IsString()) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name"}}, c->i18n);
                return;
            }
            v8::String::Utf8Value name(args.GetIsolate(), args[0]);
            bool result = c->session->saveAs(*name, *c->buffers);
            V8Response::ok(args, result);
        }, ext).ToLocalChecked()
    ).Check();

    // editor.session.loadFrom(name) -> {ok, data: {documents, activeIndex} | null, ...}
    // Adlandirilmis oturumu yukle
    jsSession->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "loadFrom"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<SessionBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!c || !c->session) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "sessionManager"}}, c ? c->i18n : nullptr);
                return;
            }
            if (args.Length() < 1 || !args[0]->IsString()) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name"}}, c->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            v8::String::Utf8Value name(iso, args[0]);

            SessionState state;
            if (!c->session->loadFrom(*name, state)) {
                V8Response::ok(args, nullptr);
                return;
            }

            json docs = json::array();
            for (size_t i = 0; i < state.documents.size(); ++i) {
                // loadFrom does not include scrollTop in the original
                // loadFrom orijinalinde scrollTop dahil degil
                json doc = {
                    {"filePath", state.documents[i].filePath},
                    {"cursorLine", state.documents[i].cursorLine},
                    {"cursorCol", state.documents[i].cursorCol}
                };
                docs.push_back(doc);
            }

            json data = {
                {"activeIndex", state.activeIndex},
                {"documents", docs}
            };
            V8Response::ok(args, data);
        }, ext).ToLocalChecked()
    ).Check();

    // editor.session.list() -> {ok, data: [string, ...], meta: {total: N}, ...}
    // Kaydedilmis oturumlari listele
    jsSession->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<SessionBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!c || !c->session) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "sessionManager"}}, c ? c->i18n : nullptr);
                return;
            }

            auto names = c->session->listSessions();
            json arr = json::array();
            for (size_t i = 0; i < names.size(); ++i) {
                arr.push_back(names[i]);
            }
            json meta = {{"total", names.size()}};
            V8Response::ok(args, arr, meta);
        }, ext).ToLocalChecked()
    ).Check();

    // editor.session.remove(name) -> {ok, data: bool, ...}
    // Adlandirilmis oturumu sil
    jsSession->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "remove"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<SessionBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!c || !c->session) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "sessionManager"}}, c ? c->i18n : nullptr);
                return;
            }
            if (args.Length() < 1 || !args[0]->IsString()) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name"}}, c->i18n);
                return;
            }
            v8::String::Utf8Value name(args.GetIsolate(), args[0]);
            bool result = c->session->deleteSession(*name);
            V8Response::ok(args, result);
        }, ext).ToLocalChecked()
    ).Check();

    // editor.session.setSessionPath(path) - Set the session file path
    // Oturum dosyasi yolunu ayarla
    jsSession->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setSessionPath"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<SessionBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!c || !c->session) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "sessionManager"}}, c ? c->i18n : nullptr);
                return;
            }
            if (args.Length() < 1 || !args[0]->IsString()) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "path"}}, c->i18n);
                return;
            }
            v8::String::Utf8Value path(args.GetIsolate(), args[0]);
            c->session->setSessionPath(*path);
            V8Response::ok(args, true);
        }, ext).ToLocalChecked()
    ).Check();

    // editor.session.lastState() -> {ok, data: {documents, activeIndex, workingDir, windowWidth, windowHeight}, ...}
    // Son kaydedilen oturum durumunu al
    jsSession->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "lastState"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<SessionBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!c || !c->session) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "sessionManager"}}, c ? c->i18n : nullptr);
                return;
            }

            const SessionState& state = c->session->lastState();

            json docs = json::array();
            for (size_t i = 0; i < state.documents.size(); ++i) {
                docs.push_back(sessionDocToJson(state.documents[i], true, true));
            }

            json data = {
                {"activeIndex", state.activeIndex},
                {"workingDir", state.lastWorkingDir},
                {"windowWidth", state.windowWidth},
                {"windowHeight", state.windowHeight},
                {"documents", docs}
            };
            V8Response::ok(args, data);
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
