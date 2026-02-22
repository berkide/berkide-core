// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "CompletionBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "CompletionEngine.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Context for completion binding
// Tamamlama binding baglami
struct CompletionCtx {
    CompletionEngine* engine;
    I18n* i18n;
};

// Helper: convert CompletionItem to nlohmann::json
// Yardimci: CompletionItem'i nlohmann::json'a cevir
static json itemToJson(const CompletionItem& item) {
    json posArr = json::array();
    for (auto pos : item.matchPositions) {
        posArr.push_back(pos);
    }
    return json({
        {"text", item.text},
        {"label", item.label},
        {"detail", item.detail},
        {"kind", item.kind},
        {"insertText", item.insertText},
        {"score", item.score},
        {"matchPositions", posArr}
    });
}

// Register editor.completion JS object with standard response format
// Standart yanit formatiyla editor.completion JS nesnesini kaydet
void RegisterCompletionBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsComp = v8::Object::New(isolate);

    auto* cctx = new CompletionCtx{edCtx.completionEngine, edCtx.i18n};

    // completion.filter(candidates, query) -> {ok, data: [item, ...], meta: {total: N}}
    // Adaylari filtrele ve puanla
    jsComp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "filter"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CompletionCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "completionEngine"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2 || !args[0]->IsArray()) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "candidates, query"}}, cc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            // Parse candidates from JS array
            // JS dizisinden adaylari ayristir
            auto jsArr = args[0].As<v8::Array>();
            std::vector<CompletionItem> candidates;
            candidates.reserve(jsArr->Length());

            for (uint32_t i = 0; i < jsArr->Length(); ++i) {
                auto elem = jsArr->Get(ctx, i).ToLocalChecked();
                CompletionItem item;

                if (elem->IsString()) {
                    // Simple string candidate
                    // Basit dize adayi
                    item.text = v8Str(iso, elem);
                    item.label = item.text;
                } else if (elem->IsObject()) {
                    auto obj = elem.As<v8::Object>();
                    auto textKey = v8::String::NewFromUtf8Literal(iso, "text");
                    auto labelKey = v8::String::NewFromUtf8Literal(iso, "label");
                    auto detailKey = v8::String::NewFromUtf8Literal(iso, "detail");
                    auto kindKey = v8::String::NewFromUtf8Literal(iso, "kind");
                    auto insertKey = v8::String::NewFromUtf8Literal(iso, "insertText");

                    if (obj->Has(ctx, textKey).FromMaybe(false))
                        item.text = v8Str(iso, obj->Get(ctx, textKey).ToLocalChecked());
                    if (obj->Has(ctx, labelKey).FromMaybe(false))
                        item.label = v8Str(iso, obj->Get(ctx, labelKey).ToLocalChecked());
                    if (obj->Has(ctx, detailKey).FromMaybe(false))
                        item.detail = v8Str(iso, obj->Get(ctx, detailKey).ToLocalChecked());
                    if (obj->Has(ctx, kindKey).FromMaybe(false))
                        item.kind = v8Str(iso, obj->Get(ctx, kindKey).ToLocalChecked());
                    if (obj->Has(ctx, insertKey).FromMaybe(false))
                        item.insertText = v8Str(iso, obj->Get(ctx, insertKey).ToLocalChecked());

                    if (item.label.empty()) item.label = item.text;
                }

                candidates.push_back(std::move(item));
            }

            std::string query = v8Str(iso, args[1]);
            auto results = cc->engine->filter(candidates, query);

            json arr = json::array();
            for (const auto& r : results) {
                arr.push_back(itemToJson(r));
            }
            json meta = {{"total", results.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // completion.score(text, query) -> {ok, data: number}
    // Tek bir metni sorguya karsi puanla
    jsComp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "score"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CompletionCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "completionEngine"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "text, query"}}, cc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();

            std::string text  = v8Str(iso, args[0]);
            std::string query = v8Str(iso, args[1]);
            double s = cc->engine->score(text, query);
            V8Response::ok(args, s);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // completion.extractWords(text) -> {ok, data: [string, ...], meta: {total: N}}
    // Metinden kelimeleri cikar
    jsComp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "extractWords"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CompletionCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "text"}}, cc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();

            std::string text = v8Str(iso, args[0]);
            auto words = CompletionEngine::extractWords(text);

            json arr = json::array();
            for (const auto& w : words) {
                arr.push_back(w);
            }
            json meta = {{"total", words.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // completion.setMaxResults(n) -> {ok, data: true}
    // Maksimum sonuc sayisini ayarla
    jsComp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setMaxResults"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CompletionCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "completionEngine"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "n"}}, cc->i18n);
                return;
            }
            int n = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromJust();
            cc->engine->setMaxResults(n);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // completion.maxResults() -> {ok, data: int}
    // Maksimum sonuc sayisini al
    jsComp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "maxResults"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CompletionCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "completionEngine"}}, cc ? cc->i18n : nullptr);
                return;
            }
            V8Response::ok(args, cc->engine->maxResults());
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "completion"),
        jsComp).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _completionReg = [] {
    BindingRegistry::instance().registerBinding("completion", RegisterCompletionBinding);
    return true;
}();
