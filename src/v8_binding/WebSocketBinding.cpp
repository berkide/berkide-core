#include "WebSocketBinding.h"
#include "WebSocketServer.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "Logger.h"
#include <v8.h>

// Register WebSocket server API on editor.ws JS object (listen, broadcast, stop)
// editor.ws JS nesnesine WebSocket sunucu API'sini kaydet (listen, broadcast, stop)
void RegisterWebSocketBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx)
{
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsWS = v8::Object::New(isolate);
    WebSocketServer* server = ctx.wsServer;

    // ws.listen(port): start the WebSocket server on the given port
    // ws.listen(port): verilen portta WebSocket sunucusunu baslat
    jsWS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listen"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1 || !args[0]->IsInt32()) return;
            int port = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(1882);
            auto* srv = static_cast<WebSocketServer*>(args.Data().As<v8::External>()->Value());
            srv->start(port);
        }, v8::External::New(isolate, server)).ToLocalChecked()
    ).Check();

    // ws.broadcast(msg): send a message to all connected WebSocket clients
    // ws.broadcast(msg): tum bagli WebSocket istemcilerine mesaj gonder
    jsWS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "broadcast"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            v8::String::Utf8Value msg(args.GetIsolate(), args[0]);
            auto* srv = static_cast<WebSocketServer*>(args.Data().As<v8::External>()->Value());
            srv->broadcast(*msg);
        }, v8::External::New(isolate, server)).ToLocalChecked()
    ).Check();

    // ws.stop(): stop the WebSocket server and disconnect all clients
    // ws.stop(): WebSocket sunucusunu durdur ve tum istemcilerin baglantilarini kes
    jsWS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "stop"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* srv = static_cast<WebSocketServer*>(args.Data().As<v8::External>()->Value());
            srv->stop();
        }, v8::External::New(isolate, server)).ToLocalChecked()
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
