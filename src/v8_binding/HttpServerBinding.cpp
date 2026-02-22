// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "HttpServerBinding.h"
#include "HttpServer.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "Logger.h"
#include <v8.h>

// Context struct to pass HTTP server pointer and i18n to lambda callbacks
// Lambda callback'lere hem HTTP sunucu hem i18n isaretcisini aktarmak icin baglam yapisi
struct HttpCtx {
    HttpServer* server;
    I18n* i18n;
};

// Register HTTP server API on editor.http JS object (listen, stop)
// editor.http JS nesnesine HTTP sunucu API'sini kaydet (listen, stop)
void RegisterHttpServerBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx)
{
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsHttp = v8::Object::New(isolate);

    auto* hctx = new HttpCtx{ctx.httpServer, ctx.i18n};

    // http.listen(port): start the HTTP REST API server on the given port
    // http.listen(port): verilen portta HTTP REST API sunucusunu baslat
    jsHttp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listen"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* hc = static_cast<HttpCtx*>(args.Data().As<v8::External>()->Value());
            if (!hc || !hc->server) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, hc ? hc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1 || !args[0]->IsInt32()) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "port"}}, hc->i18n);
                return;
            }
            int port = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(1881);
            hc->server->start(port);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, hctx)).ToLocalChecked()
    ).Check();

    // http.stop(): stop the running HTTP server
    // http.stop(): calisan HTTP sunucusunu durdur
    jsHttp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "stop"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* hc = static_cast<HttpCtx*>(args.Data().As<v8::External>()->Value());
            if (!hc || !hc->server) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, hc ? hc->i18n : nullptr);
                return;
            }
            hc->server->stop();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, hctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "http"),
        jsHttp).Check();

    LOG_INFO("[V8] Http API bound.");
}

// Auto-register "http" binding at static init time so it is applied when editor object is created
// "http" binding'ini statik baslangicta otomatik kaydet, editor nesnesi olusturulurken uygulansin
static bool registered_http = []{
    BindingRegistry::instance().registerBinding("http",
        [](v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx){
            RegisterHttpServerBinding(isolate, editorObj, ctx);
        });
    return true;
}();
