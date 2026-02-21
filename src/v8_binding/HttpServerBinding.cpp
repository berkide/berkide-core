#include "HttpServerBinding.h"
#include "HttpServer.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "Logger.h"
#include <v8.h>

// Register HTTP server API on editor.http JS object (listen, stop)
// editor.http JS nesnesine HTTP sunucu API'sini kaydet (listen, stop)
void RegisterHttpServerBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx)
{
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsHttp = v8::Object::New(isolate);
    HttpServer* server = ctx.httpServer;

    // http.listen(port): start the HTTP REST API server on the given port
    // http.listen(port): verilen portta HTTP REST API sunucusunu baslat
    jsHttp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listen"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1 || !args[0]->IsInt32()) return;
            int port = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(1881);
            auto* srv = static_cast<HttpServer*>(args.Data().As<v8::External>()->Value());
            srv->start(port);
        }, v8::External::New(isolate, server)).ToLocalChecked()
    ).Check();

    // http.stop(): stop the running HTTP server
    // http.stop(): calisan HTTP sunucusunu durdur
    jsHttp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "stop"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* srv = static_cast<HttpServer*>(args.Data().As<v8::External>()->Value());
            srv->stop();
        }, v8::External::New(isolate, server)).ToLocalChecked()
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
