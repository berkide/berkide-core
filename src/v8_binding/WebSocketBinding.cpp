// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "WebSocketBinding.h"
#include "WebSocketServer.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "Logger.h"
#include <v8.h>

// Context struct to pass WebSocket server pointer and i18n to lambda callbacks
// Lambda callback'lere hem WebSocket sunucu hem i18n isaretcisini aktarmak icin baglam yapisi
struct WsCtx {
    WebSocketServer* server;
    I18n* i18n;
};

// Register WebSocket server API on editor.ws JS object (listen, broadcast, stop)
// editor.ws JS nesnesine WebSocket sunucu API'sini kaydet (listen, broadcast, stop)
void RegisterWebSocketBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx)
{
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsWS = v8::Object::New(isolate);

    auto* wctx = new WsCtx{ctx.wsServer, ctx.i18n};

    // ws.listen(port): start the WebSocket server on the given port
    // ws.listen(port): verilen portta WebSocket sunucusunu baslat
    jsWS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listen"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WsCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->server) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, wc ? wc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1 || !args[0]->IsInt32()) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "port"}}, wc->i18n);
                return;
            }
            int port = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(1882);
            wc->server->start(port);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // ws.broadcast(msg): send a message to all connected WebSocket clients
    // ws.broadcast(msg): tum bagli WebSocket istemcilerine mesaj gonder
    jsWS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "broadcast"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WsCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->server) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, wc ? wc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "msg"}}, wc->i18n);
                return;
            }
            v8::String::Utf8Value msg(args.GetIsolate(), args[0]);
            wc->server->broadcast(*msg);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // ws.stop(): stop the WebSocket server and disconnect all clients
    // ws.stop(): WebSocket sunucusunu durdur ve tum istemcilerin baglantilarini kes
    jsWS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "stop"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WsCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->server) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, wc ? wc->i18n : nullptr);
                return;
            }
            wc->server->stop();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "ws"),
        jsWS
    ).Check();

    LOG_INFO("[V8] WebSocket API bound");
}

// Auto-register "ws" binding at static init time so it is applied when editor object is created
// "ws" binding'ini statik baslangicta otomatik kaydet, editor nesnesi olusturulurken uygulansin
static bool registered_ws = []{
    BindingRegistry::instance().registerBinding("ws",
        [](v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx){
            RegisterWebSocketBinding(isolate, editorObj, ctx);
        });
    return true;
}();
