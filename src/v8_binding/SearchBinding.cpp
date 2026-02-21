#include "SearchBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "buffers.h"
#include "state.h"
#include "SearchEngine.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Helper: extract SearchOptions from a JS options object
// Yardimci: JS secenekler nesnesinden SearchOptions cikar
static SearchOptions extractOpts(v8::Isolate* iso, v8::Local<v8::Context> ctx,
                                  const v8::FunctionCallbackInfo<v8::Value>& args, int optIdx) {
    SearchOptions opts;
    if (args.Length() > optIdx && args[optIdx]->IsObject()) {
        auto obj = args[optIdx].As<v8::Object>();
        auto csKey = v8::String::NewFromUtf8Literal(iso, "caseSensitive");
        auto reKey = v8::String::NewFromUtf8Literal(iso, "regex");
        auto wwKey = v8::String::NewFromUtf8Literal(iso, "wholeWord");
        auto waKey = v8::String::NewFromUtf8Literal(iso, "wrapAround");

        if (obj->Has(ctx, csKey).FromMaybe(false))
            opts.caseSensitive = obj->Get(ctx, csKey).ToLocalChecked()->BooleanValue(iso);
        if (obj->Has(ctx, reKey).FromMaybe(false))
            opts.regex = obj->Get(ctx, reKey).ToLocalChecked()->BooleanValue(iso);
        if (obj->Has(ctx, wwKey).FromMaybe(false))
            opts.wholeWord = obj->Get(ctx, wwKey).ToLocalChecked()->BooleanValue(iso);
        if (obj->Has(ctx, waKey).FromMaybe(false))
            opts.wrapAround = obj->Get(ctx, waKey).ToLocalChecked()->BooleanValue(iso);
    }
    return opts;
}

// Helper: convert SearchMatch to V8 object
// Yardimci: SearchMatch'i V8 nesnesine cevir
static v8::Local<v8::Object> matchToV8(v8::Isolate* iso, v8::Local<v8::Context> ctx,
                                         const SearchMatch& m) {
    v8::Local<v8::Object> obj = v8::Object::New(iso);
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "line"),
        v8::Integer::New(iso, m.line)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "col"),
        v8::Integer::New(iso, m.col)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "endCol"),
        v8::Integer::New(iso, m.endCol)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "length"),
        v8::Integer::New(iso, m.length)).Check();
    return obj;
}

// Register editor.search JS object
// editor.search JS nesnesini kaydet
void RegisterSearchBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsSearch = v8::Object::New(isolate);

    // Pack context pointers into a single external
    // Baglam isaretcilerini tek bir external'a paketle
    struct SearchCtx {
        Buffers* bufs;
        SearchEngine* engine;
    };
    auto* sctx = new SearchCtx{edCtx.buffers, edCtx.searchEngine};

    // search.find(pattern, opts?) -> match | null
    // Mevcut imlec konumundan ileri ara
    jsSearch->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "find"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SearchCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs || !sc->engine || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string pattern = v8Str(iso, args[0]);
            SearchOptions opts = extractOpts(iso, ctx, args, 1);

            auto& st = sc->bufs->active();
            auto& cur = st.getCursor();

            sc->engine->setLastPattern(pattern);
            sc->engine->setLastOptions(opts);

            SearchMatch match;
            if (sc->engine->findForward(st.getBuffer(), pattern,
                                         cur.getLine(), cur.getCol() + 1, match, opts)) {
                cur.setPosition(match.line, match.col);
                args.GetReturnValue().Set(matchToV8(iso, ctx, match));
            } else {
                args.GetReturnValue().SetNull();
            }
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // search.findNext() -> match | null - Find next using last pattern
    // Son kalibi kullanarak sonrakini bul
    jsSearch->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "findNext"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SearchCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs || !sc->engine) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            const std::string& pattern = sc->engine->lastPattern();
            if (pattern.empty()) { args.GetReturnValue().SetNull(); return; }

            auto& st = sc->bufs->active();
            auto& cur = st.getCursor();

            SearchMatch match;
            if (sc->engine->findForward(st.getBuffer(), pattern,
                                         cur.getLine(), cur.getCol() + 1,
                                         match, sc->engine->lastOptions())) {
                cur.setPosition(match.line, match.col);
                args.GetReturnValue().Set(matchToV8(iso, ctx, match));
            } else {
                args.GetReturnValue().SetNull();
            }
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // search.findPrev() -> match | null - Find previous using last pattern
    // Son kalibi kullanarak oncekini bul
    jsSearch->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "findPrev"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SearchCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs || !sc->engine) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            const std::string& pattern = sc->engine->lastPattern();
            if (pattern.empty()) { args.GetReturnValue().SetNull(); return; }

            auto& st = sc->bufs->active();
            auto& cur = st.getCursor();

            SearchMatch match;
            if (sc->engine->findBackward(st.getBuffer(), pattern,
                                          cur.getLine(), cur.getCol(),
                                          match, sc->engine->lastOptions())) {
                cur.setPosition(match.line, match.col);
                args.GetReturnValue().Set(matchToV8(iso, ctx, match));
            } else {
                args.GetReturnValue().SetNull();
            }
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // search.findAll(pattern, opts?) -> [match, ...]
    // Tum eslemeleri bul
    jsSearch->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "findAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SearchCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs || !sc->engine || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string pattern = v8Str(iso, args[0]);
            SearchOptions opts = extractOpts(iso, ctx, args, 1);

            auto matches = sc->engine->findAll(sc->bufs->active().getBuffer(), pattern, opts);
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(matches.size()));
            for (size_t i = 0; i < matches.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i), matchToV8(iso, ctx, matches[i])).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // search.replace(pattern, replacement, opts?) -> match | null
    // Ilk eslemeyi degistir ve sonraki eslemeyi dondur
    jsSearch->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "replace"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SearchCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs || !sc->engine || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string pattern = v8Str(iso, args[0]);
            std::string replacement = v8Str(iso, args[1]);
            SearchOptions opts = extractOpts(iso, ctx, args, 2);

            auto& st = sc->bufs->active();
            auto& cur = st.getCursor();

            SearchMatch nextMatch;
            if (sc->engine->replaceNext(st.getBuffer(), pattern, replacement,
                                         cur.getLine(), cur.getCol(), nextMatch, opts)) {
                st.markModified(true);
                cur.setPosition(nextMatch.line, nextMatch.col);
                args.GetReturnValue().Set(matchToV8(iso, ctx, nextMatch));
            } else {
                args.GetReturnValue().SetNull();
            }
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // search.replaceAll(pattern, replacement, opts?) -> number of replacements
    // Tum oluslari degistir, degistirme sayisini dondur
    jsSearch->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "replaceAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SearchCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs || !sc->engine || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string pattern = v8Str(iso, args[0]);
            std::string replacement = v8Str(iso, args[1]);
            SearchOptions opts = extractOpts(iso, ctx, args, 2);

            auto& st = sc->bufs->active();
            int count = sc->engine->replaceAll(st.getBuffer(), pattern, replacement, opts);
            if (count > 0) st.markModified(true);
            args.GetReturnValue().Set(v8::Integer::New(iso, count));
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // search.count(pattern, opts?) -> number of matches
    // Esleme sayisini dondur
    jsSearch->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "count"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SearchCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs || !sc->engine || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string pattern = v8Str(iso, args[0]);
            SearchOptions opts = extractOpts(iso, ctx, args, 1);
            int count = sc->engine->countMatches(sc->bufs->active().getBuffer(), pattern, opts);
            args.GetReturnValue().Set(v8::Integer::New(iso, count));
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "search"),
        jsSearch).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _searchReg = [] {
    BindingRegistry::instance().registerBinding("search", RegisterSearchBinding);
    return true;
}();
