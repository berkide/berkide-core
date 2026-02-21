#include "HelpBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "HelpSystem.h"
#include <v8.h>

// Helper: convert HelpTopic to V8 object
// Yardimci: HelpTopic'i V8 nesnesine cevir
static v8::Local<v8::Object> topicToV8(v8::Isolate* isolate, const HelpTopic* topic) {
    auto ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> obj = v8::Object::New(isolate);

    obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "id"),
        v8::String::NewFromUtf8(isolate, topic->id.c_str()).ToLocalChecked()).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "title"),
        v8::String::NewFromUtf8(isolate, topic->title.c_str()).ToLocalChecked()).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "content"),
        v8::String::NewFromUtf8(isolate, topic->content.c_str()).ToLocalChecked()).Check();

    v8::Local<v8::Array> tags = v8::Array::New(isolate, static_cast<int>(topic->tags.size()));
    for (size_t i = 0; i < topic->tags.size(); ++i) {
        tags->Set(ctx, static_cast<uint32_t>(i),
            v8::String::NewFromUtf8(isolate, topic->tags[i].c_str()).ToLocalChecked()).Check();
    }
    obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "tags"), tags).Check();

    return obj;
}

// Register editor.help JS object with show(topic), search(query), list()
// editor.help JS nesnesini show(topic), search(query), list() ile kaydet
void RegisterHelpBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsHelp = v8::Object::New(isolate);
    HelpSystem* hs = ctx.helpSystem;

    // help.show(topicId) -> {id, title, content, tags} or undefined
    jsHelp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "show"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            auto* hs = static_cast<HelpSystem*>(args.Data().As<v8::External>()->Value());
            if (!hs) return;
            v8::String::Utf8Value id(args.GetIsolate(), args[0]);
            auto* topic = hs->getTopic(*id ? *id : "");
            if (!topic) return;
            args.GetReturnValue().Set(topicToV8(args.GetIsolate(), topic));
        }, v8::External::New(isolate, hs)).ToLocalChecked()
    ).Check();

    // help.search(query) -> [{id, title, content, tags}, ...]
    jsHelp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "search"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            auto* hs = static_cast<HelpSystem*>(args.Data().As<v8::External>()->Value());
            if (!hs) return;
            auto* isolate = args.GetIsolate();
            auto ctx = isolate->GetCurrentContext();
            v8::String::Utf8Value query(isolate, args[0]);
            auto results = hs->search(*query ? *query : "");

            v8::Local<v8::Array> arr = v8::Array::New(isolate, static_cast<int>(results.size()));
            for (size_t i = 0; i < results.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i), topicToV8(isolate, results[i])).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, hs)).ToLocalChecked()
    ).Check();

    // help.list() -> [{id, title, tags}, ...] (without full content for efficiency)
    jsHelp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* hs = static_cast<HelpSystem*>(args.Data().As<v8::External>()->Value());
            if (!hs) return;
            auto* isolate = args.GetIsolate();
            auto ctx = isolate->GetCurrentContext();
            auto topics = hs->listTopics();

            v8::Local<v8::Array> arr = v8::Array::New(isolate, static_cast<int>(topics.size()));
            for (size_t i = 0; i < topics.size(); ++i) {
                v8::Local<v8::Object> obj = v8::Object::New(isolate);
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "id"),
                    v8::String::NewFromUtf8(isolate, topics[i]->id.c_str()).ToLocalChecked()).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "title"),
                    v8::String::NewFromUtf8(isolate, topics[i]->title.c_str()).ToLocalChecked()).Check();

                v8::Local<v8::Array> tags = v8::Array::New(isolate, static_cast<int>(topics[i]->tags.size()));
                for (size_t j = 0; j < topics[i]->tags.size(); ++j) {
                    tags->Set(ctx, static_cast<uint32_t>(j),
                        v8::String::NewFromUtf8(isolate, topics[i]->tags[j].c_str()).ToLocalChecked()).Check();
                }
                obj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "tags"), tags).Check();

                arr->Set(ctx, static_cast<uint32_t>(i), obj).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, hs)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "help"),
        jsHelp).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _helpReg = [] {
    BindingRegistry::instance().registerBinding("help", RegisterHelpBinding);
    return true;
}();
