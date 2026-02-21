#include "CompletionBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "CompletionEngine.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Helper: convert CompletionItem to V8 object
// Yardimci: CompletionItem'i V8 nesnesine cevir
static v8::Local<v8::Object> itemToV8(v8::Isolate* iso, v8::Local<v8::Context> ctx,
                                        const CompletionItem& item) {
    v8::Local<v8::Object> obj = v8::Object::New(iso);
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "text"),
        v8::String::NewFromUtf8(iso, item.text.c_str()).ToLocalChecked()).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "label"),
        v8::String::NewFromUtf8(iso, item.label.c_str()).ToLocalChecked()).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "detail"),
        v8::String::NewFromUtf8(iso, item.detail.c_str()).ToLocalChecked()).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "kind"),
        v8::String::NewFromUtf8(iso, item.kind.c_str()).ToLocalChecked()).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "insertText"),
        v8::String::NewFromUtf8(iso, item.insertText.c_str()).ToLocalChecked()).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "score"),
        v8::Number::New(iso, item.score)).Check();

    // Match positions array
    // Esleme konumlari dizisi
    v8::Local<v8::Array> posArr = v8::Array::New(iso, static_cast<int>(item.matchPositions.size()));
    for (size_t i = 0; i < item.matchPositions.size(); ++i) {
        posArr->Set(ctx, static_cast<uint32_t>(i),
            v8::Integer::New(iso, item.matchPositions[i])).Check();
    }
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "matchPositions"), posArr).Check();

    return obj;
}

// Register editor.completion JS object
// editor.completion JS nesnesini kaydet
void RegisterCompletionBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsComp = v8::Object::New(isolate);

    auto* eng = edCtx.completionEngine;

    // completion.filter(candidates, query) -> [item, ...]
    // Adaylari filtrele ve puanla
    jsComp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "filter"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ce = static_cast<CompletionEngine*>(args.Data().As<v8::External>()->Value());
            if (!ce || args.Length() < 2 || !args[0]->IsArray()) return;
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
            auto results = ce->filter(candidates, query);

            v8::Local<v8::Array> resultArr = v8::Array::New(iso, static_cast<int>(results.size()));
            for (size_t i = 0; i < results.size(); ++i) {
                resultArr->Set(ctx, static_cast<uint32_t>(i), itemToV8(iso, ctx, results[i])).Check();
            }
            args.GetReturnValue().Set(resultArr);
        }, v8::External::New(isolate, eng)).ToLocalChecked()
    ).Check();

    // completion.score(text, query) -> number
    // Tek bir metni sorguya karsi puanla
    jsComp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "score"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ce = static_cast<CompletionEngine*>(args.Data().As<v8::External>()->Value());
            if (!ce || args.Length() < 2) return;
            auto* iso = args.GetIsolate();

            std::string text  = v8Str(iso, args[0]);
            std::string query = v8Str(iso, args[1]);
            double s = ce->score(text, query);
            args.GetReturnValue().Set(v8::Number::New(iso, s));
        }, v8::External::New(isolate, eng)).ToLocalChecked()
    ).Check();

    // completion.extractWords(text) -> [string, ...]
    // Metinden kelimeleri cikar
    jsComp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "extractWords"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string text = v8Str(iso, args[0]);
            auto words = CompletionEngine::extractWords(text);

            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(words.size()));
            for (size_t i = 0; i < words.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i),
                    v8::String::NewFromUtf8(iso, words[i].c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, eng)).ToLocalChecked()
    ).Check();

    // completion.setMaxResults(n)
    // Maksimum sonuc sayisini ayarla
    jsComp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setMaxResults"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ce = static_cast<CompletionEngine*>(args.Data().As<v8::External>()->Value());
            if (!ce || args.Length() < 1) return;
            int n = args[0]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromJust();
            ce->setMaxResults(n);
        }, v8::External::New(isolate, eng)).ToLocalChecked()
    ).Check();

    // completion.maxResults() -> int - Get maximum number of results
    // Maksimum sonuc sayisini al
    jsComp->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "maxResults"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ce = static_cast<CompletionEngine*>(args.Data().As<v8::External>()->Value());
            if (!ce) return;
            args.GetReturnValue().Set(ce->maxResults());
        }, v8::External::New(isolate, eng)).ToLocalChecked()
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
