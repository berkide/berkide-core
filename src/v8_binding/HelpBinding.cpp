// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "HelpBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "HelpSystem.h"
#include <v8.h>

// Helper: convert HelpTopic to nlohmann::json
// Yardimci: HelpTopic'i nlohmann::json'a cevir
static json topicToJson(const HelpTopic* topic) {
    json tags = json::array();
    for (const auto& t : topic->tags) {
        tags.push_back(t);
    }
    return json({
        {"id", topic->id},
        {"title", topic->title},
        {"content", topic->content},
        {"tags", tags}
    });
}

// Helper: convert HelpTopic to summary json (without content for efficiency)
// Yardimci: HelpTopic'i ozet json'a cevir (verimlilik icin icerik olmadan)
static json topicSummaryToJson(const HelpTopic* topic) {
    json tags = json::array();
    for (const auto& t : topic->tags) {
        tags.push_back(t);
    }
    return json({
        {"id", topic->id},
        {"title", topic->title},
        {"tags", tags}
    });
}

// Context struct to pass help system pointer and i18n to lambda callbacks
// Lambda callback'lere hem yardim sistemi hem i18n isaretcisini aktarmak icin baglam yapisi
struct HelpCtx {
    HelpSystem* hs;
    I18n* i18n;
};

// Register editor.help JS object with show(topic), search(query), list()
// editor.help JS nesnesini show(topic), search(query), list() ile kaydet
void RegisterHelpBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsHelp = v8::Object::New(isolate);

    auto* hctx = new HelpCtx{ctx.helpSystem, ctx.i18n};

    // help.show(topicId) -> {ok, data: {id, title, content, tags}, ...}
    // Yardim konusunu goster
    jsHelp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "show"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* hc = static_cast<HelpCtx*>(args.Data().As<v8::External>()->Value());
            if (!hc || !hc->hs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, hc ? hc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "topicId"}}, hc->i18n);
                return;
            }
            v8::String::Utf8Value id(args.GetIsolate(), args[0]);
            std::string topicId = *id ? *id : "";
            auto* topic = hc->hs->getTopic(topicId);
            if (!topic) {
                V8Response::error(args, "NOT_FOUND", "help.topic.not_found",
                    {{"id", topicId}}, hc->i18n);
                return;
            }
            V8Response::ok(args, topicToJson(topic));
        }, v8::External::New(isolate, hctx)).ToLocalChecked()
    ).Check();

    // help.search(query) -> {ok, data: [{id, title, content, tags}, ...], meta: {total: N}}
    // Yardim konularinda ara
    jsHelp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "search"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* hc = static_cast<HelpCtx*>(args.Data().As<v8::External>()->Value());
            if (!hc || !hc->hs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, hc ? hc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "query"}}, hc->i18n);
                return;
            }
            v8::String::Utf8Value query(args.GetIsolate(), args[0]);
            auto results = hc->hs->search(*query ? *query : "");

            json arr = json::array();
            for (const auto* topic : results) {
                arr.push_back(topicToJson(topic));
            }
            json meta = {{"total", results.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, hctx)).ToLocalChecked()
    ).Check();

    // help.list() -> {ok, data: [{id, title, tags}, ...], meta: {total: N}}
    // Tum yardim konularini listele (verimlilik icin icerik olmadan)
    jsHelp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* hc = static_cast<HelpCtx*>(args.Data().As<v8::External>()->Value());
            if (!hc || !hc->hs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, hc ? hc->i18n : nullptr);
                return;
            }
            auto topics = hc->hs->listTopics();

            json arr = json::array();
            for (const auto* topic : topics) {
                arr.push_back(topicSummaryToJson(topic));
            }
            json meta = {{"total", topics.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, hctx)).ToLocalChecked()
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
