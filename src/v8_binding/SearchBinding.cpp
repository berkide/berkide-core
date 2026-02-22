// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "SearchBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
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

// Helper: convert SearchMatch to nlohmann::json
// Yardimci: SearchMatch'i nlohmann::json'a cevir
static json matchToJson(const SearchMatch& m) {
    return json({
        {"line", m.line},
        {"col", m.col},
        {"endCol", m.endCol},
        {"length", m.length}
    });
}

// Context struct for search binding lambdas
// Arama binding lambda'lari icin baglam yapisi
struct SearchCtx {
    Buffers* bufs;
    SearchEngine* engine;
    I18n* i18n;
};

// Register editor.search JS object with standard response format
// Standart yanit formatiyla editor.search JS nesnesini kaydet
void RegisterSearchBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsSearch = v8::Object::New(isolate);

    auto* sctx = new SearchCtx{edCtx.buffers, edCtx.searchEngine, edCtx.i18n};

    // search.find(pattern, opts?) -> {ok, data: match|null, ...}
    // Mevcut imlec konumundan ileri ara
    jsSearch->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "find"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SearchCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs || !sc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, sc ? sc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "pattern"}}, sc->i18n);
                return;
            }
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
                V8Response::ok(args, matchToJson(match), nullptr,
                    "search.find.success",
                    {{"line", std::to_string(match.line)}, {"col", std::to_string(match.col)}},
                    sc->i18n);
            } else {
                V8Response::ok(args, nullptr, nullptr,
                    "search.find.not_found", {{"pattern", pattern}}, sc->i18n);
            }
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // search.findNext() -> {ok, data: match|null, ...}
    // Son kalibi kullanarak sonrakini bul
    jsSearch->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "findNext"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SearchCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs || !sc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, sc ? sc->i18n : nullptr);
                return;
            }
            auto* iso = args.GetIsolate();

            const std::string& pattern = sc->engine->lastPattern();
            if (pattern.empty()) {
                V8Response::ok(args, nullptr, nullptr,
                    "search.find.not_found", {{"pattern", ""}}, sc->i18n);
                return;
            }

            auto& st = sc->bufs->active();
            auto& cur = st.getCursor();

            SearchMatch match;
            if (sc->engine->findForward(st.getBuffer(), pattern,
                                         cur.getLine(), cur.getCol() + 1,
                                         match, sc->engine->lastOptions())) {
                cur.setPosition(match.line, match.col);
                V8Response::ok(args, matchToJson(match), nullptr,
                    "search.find.success",
                    {{"line", std::to_string(match.line)}, {"col", std::to_string(match.col)}},
                    sc->i18n);
            } else {
                V8Response::ok(args, nullptr, nullptr,
                    "search.find.not_found", {{"pattern", pattern}}, sc->i18n);
            }
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // search.findPrev() -> {ok, data: match|null, ...}
    // Son kalibi kullanarak oncekini bul
    jsSearch->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "findPrev"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SearchCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs || !sc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, sc ? sc->i18n : nullptr);
                return;
            }
            auto* iso = args.GetIsolate();

            const std::string& pattern = sc->engine->lastPattern();
            if (pattern.empty()) {
                V8Response::ok(args, nullptr, nullptr,
                    "search.find.not_found", {{"pattern", ""}}, sc->i18n);
                return;
            }

            auto& st = sc->bufs->active();
            auto& cur = st.getCursor();

            SearchMatch match;
            if (sc->engine->findBackward(st.getBuffer(), pattern,
                                          cur.getLine(), cur.getCol(),
                                          match, sc->engine->lastOptions())) {
                cur.setPosition(match.line, match.col);
                V8Response::ok(args, matchToJson(match), nullptr,
                    "search.find.success",
                    {{"line", std::to_string(match.line)}, {"col", std::to_string(match.col)}},
                    sc->i18n);
            } else {
                V8Response::ok(args, nullptr, nullptr,
                    "search.find.not_found", {{"pattern", pattern}}, sc->i18n);
            }
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // search.findAll(pattern, opts?) -> {ok, data: [match, ...], meta: {total: N}, ...}
    // Tum eslemeleri bul
    jsSearch->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "findAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SearchCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs || !sc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, sc ? sc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "pattern"}}, sc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string pattern = v8Str(iso, args[0]);
            SearchOptions opts = extractOpts(iso, ctx, args, 1);

            auto matches = sc->engine->findAll(sc->bufs->active().getBuffer(), pattern, opts);
            json arr = json::array();
            for (const auto& m : matches) {
                arr.push_back(matchToJson(m));
            }
            json meta = {{"total", matches.size()}};
            V8Response::ok(args, arr, meta, "search.findall.success",
                {{"count", std::to_string(matches.size())}, {"pattern", pattern}}, sc->i18n);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // search.replace(pattern, replacement, opts?) -> {ok, data: match|null, ...}
    // Ilk eslemeyi degistir ve sonraki eslemeyi dondur
    jsSearch->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "replace"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SearchCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs || !sc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, sc ? sc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "pattern, replacement"}}, sc->i18n);
                return;
            }
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
                V8Response::ok(args, matchToJson(nextMatch), nullptr,
                    "search.replace.success",
                    {{"line", std::to_string(nextMatch.line)}, {"col", std::to_string(nextMatch.col)}},
                    sc->i18n);
            } else {
                V8Response::ok(args, nullptr, nullptr,
                    "search.find.not_found", {{"pattern", pattern}}, sc->i18n);
            }
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // search.replaceAll(pattern, replacement, opts?) -> {ok, data: count, meta: {total: N}, ...}
    // Tum oluslari degistir
    jsSearch->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "replaceAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SearchCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs || !sc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, sc ? sc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "pattern, replacement"}}, sc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string pattern = v8Str(iso, args[0]);
            std::string replacement = v8Str(iso, args[1]);
            SearchOptions opts = extractOpts(iso, ctx, args, 2);

            auto& st = sc->bufs->active();
            int count = sc->engine->replaceAll(st.getBuffer(), pattern, replacement, opts);
            if (count > 0) st.markModified(true);

            json meta = {{"total", count}};
            V8Response::ok(args, count, meta, "search.replaceall.success",
                {{"count", std::to_string(count)}}, sc->i18n);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // search.count(pattern, opts?) -> {ok, data: number, meta: {total: N}, ...}
    // Esleme sayisini dondur
    jsSearch->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "count"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SearchCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs || !sc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, sc ? sc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "pattern"}}, sc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string pattern = v8Str(iso, args[0]);
            SearchOptions opts = extractOpts(iso, ctx, args, 1);
            int count = sc->engine->countMatches(sc->bufs->active().getBuffer(), pattern, opts);

            json meta = {{"total", count}};
            V8Response::ok(args, count, meta, "search.count.success",
                {{"count", std::to_string(count)}, {"pattern", pattern}}, sc->i18n);
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
