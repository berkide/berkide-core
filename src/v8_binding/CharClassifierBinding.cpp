// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "CharClassifierBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "buffers.h"
#include "CharClassifier.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Context for char classifier binding
// Karakter siniflandirici binding baglami
struct CharCtx {
    EditorContext* edCtx;
    I18n* i18n;
};

// Register editor.chars JS binding with standard response format
// Standart yanit formatiyla editor.chars JS binding'ini kaydet
void RegisterCharClassifierBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto context = isolate->GetCurrentContext();
    auto charsObj = v8::Object::New(isolate);

    auto* cctx = new CharCtx{&ctx, ctx.i18n};

    // editor.chars.classify(char) -> {ok, data: "word"|"whitespace"|"punctuation"|"linebreak"|"other"}
    // Karakteri siniflandir
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "classify"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CharCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->edCtx || !cc->edCtx->charClassifier) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "charClassifier"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "char"}}, cc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            std::string s = v8Str(iso, args[0]);
            if (s.empty()) {
                V8Response::error(args, "INVALID_ARG", "args.empty_string",
                    {{"name", "char"}}, cc->i18n);
                return;
            }

            CharType type = cc->edCtx->charClassifier->classify(s[0]);
            std::string result = "other";
            switch (type) {
                case CharType::Word:        result = "word"; break;
                case CharType::Whitespace:  result = "whitespace"; break;
                case CharType::Punctuation: result = "punctuation"; break;
                case CharType::LineBreak:   result = "linebreak"; break;
                default: break;
            }
            V8Response::ok(args, result);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // editor.chars.isWord(char) -> {ok, data: bool}
    // Kelime karakteri mi kontrol et
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "isWord"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CharCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->edCtx || !cc->edCtx->charClassifier) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "charClassifier"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "char"}}, cc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            std::string s = v8Str(iso, args[0]);
            if (s.empty()) {
                V8Response::ok(args, false);
                return;
            }
            V8Response::ok(args, cc->edCtx->charClassifier->isWord(s[0]));
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // editor.chars.wordAt(line, col) -> {ok, data: {startCol, endCol, text}}
    // Konumdaki kelimeyi al
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "wordAt"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CharCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->edCtx || !cc->edCtx->charClassifier || !cc->edCtx->buffers) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "charClassifier"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "line, col"}}, cc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            int col  = args[1]->Int32Value(ctx2).FromMaybe(0);

            auto& buf = cc->edCtx->buffers->active().getBuffer();
            if (line < 0 || line >= buf.lineCount()) {
                V8Response::error(args, "OUT_OF_RANGE", "args.out_of_range",
                    {{"name", "line"}}, cc->i18n);
                return;
            }

            WordRange wr = cc->edCtx->charClassifier->wordAt(buf.getLine(line), col);
            json data = {
                {"startCol", wr.startCol},
                {"endCol", wr.endCol},
                {"text", wr.text}
            };
            V8Response::ok(args, data);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // editor.chars.nextWordStart(line, col) -> {ok, data: col}
    // Sonraki kelime baslangicini bul
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "nextWordStart"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CharCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->edCtx || !cc->edCtx->charClassifier || !cc->edCtx->buffers) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "charClassifier"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "line, col"}}, cc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            int col  = args[1]->Int32Value(ctx2).FromMaybe(0);

            auto& buf = cc->edCtx->buffers->active().getBuffer();
            if (line < 0 || line >= buf.lineCount()) {
                V8Response::error(args, "OUT_OF_RANGE", "args.out_of_range",
                    {{"name", "line"}}, cc->i18n);
                return;
            }

            int result = cc->edCtx->charClassifier->nextWordStart(buf.getLine(line), col);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // editor.chars.prevWordStart(line, col) -> {ok, data: col}
    // Onceki kelime baslangicini bul
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "prevWordStart"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CharCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->edCtx || !cc->edCtx->charClassifier || !cc->edCtx->buffers) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "charClassifier"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "line, col"}}, cc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            int col  = args[1]->Int32Value(ctx2).FromMaybe(0);

            auto& buf = cc->edCtx->buffers->active().getBuffer();
            if (line < 0 || line >= buf.lineCount()) {
                V8Response::error(args, "OUT_OF_RANGE", "args.out_of_range",
                    {{"name", "line"}}, cc->i18n);
                return;
            }

            int result = cc->edCtx->charClassifier->prevWordStart(buf.getLine(line), col);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // editor.chars.wordEnd(line, col) -> {ok, data: col}
    // Kelime sonunu bul
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "wordEnd"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CharCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->edCtx || !cc->edCtx->charClassifier || !cc->edCtx->buffers) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "charClassifier"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "line, col"}}, cc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            int col  = args[1]->Int32Value(ctx2).FromMaybe(0);

            auto& buf = cc->edCtx->buffers->active().getBuffer();
            if (line < 0 || line >= buf.lineCount()) {
                V8Response::error(args, "OUT_OF_RANGE", "args.out_of_range",
                    {{"name", "line"}}, cc->i18n);
                return;
            }

            int result = cc->edCtx->charClassifier->wordEnd(buf.getLine(line), col);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // editor.chars.matchBracket(line, col) -> {ok, data: {line, col, bracket} | null}
    // Eslesen parantezi bul
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "matchBracket"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CharCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->edCtx || !cc->edCtx->charClassifier || !cc->edCtx->buffers) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "charClassifier"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "line, col"}}, cc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            int col  = args[1]->Int32Value(ctx2).FromMaybe(0);

            auto& buf = cc->edCtx->buffers->active().getBuffer();
            BracketMatch match = cc->edCtx->charClassifier->findMatchingBracket(buf, line, col);

            if (!match.found) {
                V8Response::ok(args, nullptr);
                return;
            }

            json data = {
                {"line", match.line},
                {"col", match.col},
                {"bracket", std::string(1, match.bracket)}
            };
            V8Response::ok(args, data);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // editor.chars.addWordChar(char) -> {ok, data: true}
    // Ekstra kelime karakteri ekle
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "addWordChar"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CharCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->edCtx || !cc->edCtx->charClassifier) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "charClassifier"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "char"}}, cc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            std::string s = v8Str(iso, args[0]);
            if (!s.empty()) cc->edCtx->charClassifier->addWordChar(s[0]);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // editor.chars.addBracketPair(open, close) -> {ok, data: true}
    // Ozel parantez cifti ekle
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "addBracketPair"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CharCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->edCtx || !cc->edCtx->charClassifier) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "charClassifier"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "open, close"}}, cc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            std::string open  = v8Str(iso, args[0]);
            std::string close = v8Str(iso, args[1]);
            if (!open.empty() && !close.empty()) {
                cc->edCtx->charClassifier->addBracketPair(open[0], close[0]);
            }
            V8Response::ok(args, true);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // editor.chars.isWhitespace(char) -> {ok, data: bool}
    // Bosluk karakteri mi kontrol et
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "isWhitespace"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CharCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->edCtx || !cc->edCtx->charClassifier) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "charClassifier"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "char"}}, cc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            std::string s = v8Str(iso, args[0]);
            if (s.empty()) {
                V8Response::ok(args, false);
                return;
            }
            V8Response::ok(args, cc->edCtx->charClassifier->isWhitespace(s[0]));
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // editor.chars.isBracket(char) -> {ok, data: bool}
    // Parantez karakteri mi kontrol et
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "isBracket"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CharCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->edCtx || !cc->edCtx->charClassifier) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "charClassifier"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "char"}}, cc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            std::string s = v8Str(iso, args[0]);
            if (s.empty()) {
                V8Response::ok(args, false);
                return;
            }
            V8Response::ok(args, cc->edCtx->charClassifier->isBracket(s[0]));
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // editor.chars.isOpenBracket(char) -> {ok, data: bool}
    // Acik parantez mi kontrol et
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "isOpenBracket"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CharCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->edCtx || !cc->edCtx->charClassifier) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "charClassifier"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "char"}}, cc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            std::string s = v8Str(iso, args[0]);
            if (s.empty()) {
                V8Response::ok(args, false);
                return;
            }
            V8Response::ok(args, cc->edCtx->charClassifier->isOpenBracket(s[0]));
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // editor.chars.isCloseBracket(char) -> {ok, data: bool}
    // Kapali parantez mi kontrol et
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "isCloseBracket"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CharCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->edCtx || !cc->edCtx->charClassifier) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "charClassifier"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "char"}}, cc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            std::string s = v8Str(iso, args[0]);
            if (s.empty()) {
                V8Response::ok(args, false);
                return;
            }
            V8Response::ok(args, cc->edCtx->charClassifier->isCloseBracket(s[0]));
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // editor.chars.matchingBracketChar(char) -> {ok, data: string}
    // Eslesen parantez karakterini dondur
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "matchingBracketChar"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CharCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->edCtx || !cc->edCtx->charClassifier) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "charClassifier"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "char"}}, cc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            std::string s = v8Str(iso, args[0]);
            if (s.empty()) {
                V8Response::ok(args, std::string(""));
                return;
            }
            char m = cc->edCtx->charClassifier->matchingBracket(s[0]);
            if (m == 0) {
                V8Response::ok(args, std::string(""));
                return;
            }
            V8Response::ok(args, std::string(1, m));
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // editor.chars.removeWordChar(char) -> {ok, data: true}
    // Ekstra kelime karakterini kaldir
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "removeWordChar"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CharCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->edCtx || !cc->edCtx->charClassifier) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "charClassifier"}}, cc ? cc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "char"}}, cc->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            std::string s = v8Str(iso, args[0]);
            if (!s.empty()) cc->edCtx->charClassifier->removeWordChar(s[0]);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    // editor.chars.bracketPairs() -> {ok, data: [{open, close}, ...], meta: {total: N}}
    // Parantez ciftlerini listele
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "bracketPairs"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* cc = static_cast<CharCtx*>(args.Data().As<v8::External>()->Value());
            if (!cc || !cc->edCtx || !cc->edCtx->charClassifier) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "charClassifier"}}, cc ? cc->i18n : nullptr);
                return;
            }

            const auto& pairs = cc->edCtx->charClassifier->bracketPairs();
            json arr = json::array();
            for (const auto& p : pairs) {
                arr.push_back(json({
                    {"open", std::string(1, p.open)},
                    {"close", std::string(1, p.close)}
                }));
            }
            json meta = {{"total", pairs.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, cctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "chars"),
        charsObj).Check();
}

// Self-register at static initialization time
// Statik baslatma zamaninda kendini kaydet
static bool _charClassifierReg = [] {
    BindingRegistry::instance().registerBinding("chars", RegisterCharClassifierBinding);
    return true;
}();
