#include "CharClassifierBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "buffers.h"
#include "CharClassifier.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Register editor.chars JS binding
// editor.chars JS binding'ini kaydet
void RegisterCharClassifierBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto context = isolate->GetCurrentContext();
    auto charsObj = v8::Object::New(isolate);

    // editor.chars.classify(char) -> "word"|"whitespace"|"punctuation"|"linebreak"|"other"
    // editor.chars.siniflandir(karakter) -> "kelime"|"bosluk"|"noktalama"|"satirSonu"|"diger"
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "classify"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->charClassifier || args.Length() < 1) return;

            std::string s = v8Str(iso, args[0]);
            if (s.empty()) return;

            CharType type = extCtx->charClassifier->classify(s[0]);
            const char* result = "other";
            switch (type) {
                case CharType::Word:        result = "word"; break;
                case CharType::Whitespace:  result = "whitespace"; break;
                case CharType::Punctuation: result = "punctuation"; break;
                case CharType::LineBreak:   result = "linebreak"; break;
                default: break;
            }
            args.GetReturnValue().Set(v8::String::NewFromUtf8(iso, result).ToLocalChecked());
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.chars.isWord(char) -> bool
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "isWord"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->charClassifier || args.Length() < 1) return;
            std::string s = v8Str(iso, args[0]);
            if (s.empty()) return;
            args.GetReturnValue().Set(extCtx->charClassifier->isWord(s[0]));
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.chars.wordAt(line, col) -> {startCol, endCol, text}
    // editor.chars.kelimeAl(satir, sutun) -> {baslangicSutun, bitisSutun, metin}
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "wordAt"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->charClassifier || !extCtx->buffers || args.Length() < 2) return;

            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            int col  = args[1]->Int32Value(ctx2).FromMaybe(0);

            auto& buf = extCtx->buffers->active().getBuffer();
            if (line < 0 || line >= buf.lineCount()) return;

            WordRange wr = extCtx->charClassifier->wordAt(buf.getLine(line), col);

            auto result = v8::Object::New(iso);
            result->Set(ctx2, v8::String::NewFromUtf8Literal(iso, "startCol"),
                v8::Integer::New(iso, wr.startCol)).Check();
            result->Set(ctx2, v8::String::NewFromUtf8Literal(iso, "endCol"),
                v8::Integer::New(iso, wr.endCol)).Check();
            result->Set(ctx2, v8::String::NewFromUtf8Literal(iso, "text"),
                v8::String::NewFromUtf8(iso, wr.text.c_str()).ToLocalChecked()).Check();
            args.GetReturnValue().Set(result);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.chars.nextWordStart(line, col) -> col
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "nextWordStart"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->charClassifier || !extCtx->buffers || args.Length() < 2) return;

            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            int col  = args[1]->Int32Value(ctx2).FromMaybe(0);

            auto& buf = extCtx->buffers->active().getBuffer();
            if (line < 0 || line >= buf.lineCount()) return;

            int result = extCtx->charClassifier->nextWordStart(buf.getLine(line), col);
            args.GetReturnValue().Set(result);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.chars.prevWordStart(line, col) -> col
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "prevWordStart"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->charClassifier || !extCtx->buffers || args.Length() < 2) return;

            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            int col  = args[1]->Int32Value(ctx2).FromMaybe(0);

            auto& buf = extCtx->buffers->active().getBuffer();
            if (line < 0 || line >= buf.lineCount()) return;

            int result = extCtx->charClassifier->prevWordStart(buf.getLine(line), col);
            args.GetReturnValue().Set(result);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.chars.wordEnd(line, col) -> col
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "wordEnd"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->charClassifier || !extCtx->buffers || args.Length() < 2) return;

            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            int col  = args[1]->Int32Value(ctx2).FromMaybe(0);

            auto& buf = extCtx->buffers->active().getBuffer();
            if (line < 0 || line >= buf.lineCount()) return;

            int result = extCtx->charClassifier->wordEnd(buf.getLine(line), col);
            args.GetReturnValue().Set(result);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.chars.matchBracket(line, col) -> {found, line, col, bracket} | null
    // editor.chars.parantezEsle(satir, sutun) -> {bulundu, satir, sutun, parantez} | null
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "matchBracket"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->charClassifier || !extCtx->buffers || args.Length() < 2) return;

            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            int col  = args[1]->Int32Value(ctx2).FromMaybe(0);

            auto& buf = extCtx->buffers->active().getBuffer();
            BracketMatch match = extCtx->charClassifier->findMatchingBracket(buf, line, col);

            if (!match.found) {
                args.GetReturnValue().SetNull();
                return;
            }

            auto result = v8::Object::New(iso);
            result->Set(ctx2, v8::String::NewFromUtf8Literal(iso, "line"),
                v8::Integer::New(iso, match.line)).Check();
            result->Set(ctx2, v8::String::NewFromUtf8Literal(iso, "col"),
                v8::Integer::New(iso, match.col)).Check();
            result->Set(ctx2, v8::String::NewFromUtf8Literal(iso, "bracket"),
                v8::String::NewFromUtf8(iso, std::string(1, match.bracket).c_str()).ToLocalChecked()).Check();
            args.GetReturnValue().Set(result);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.chars.addWordChar(char) - add extra word character
    // editor.chars.kelimeKarEkle(karakter) - ekstra kelime karakteri ekle
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "addWordChar"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->charClassifier || args.Length() < 1) return;
            std::string s = v8Str(iso, args[0]);
            if (!s.empty()) extCtx->charClassifier->addWordChar(s[0]);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.chars.addBracketPair(open, close) - add custom bracket pair
    // editor.chars.parantezCiftiEkle(ac, kapa) - ozel parantez cifti ekle
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "addBracketPair"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->charClassifier || args.Length() < 2) return;
            std::string open  = v8Str(iso, args[0]);
            std::string close = v8Str(iso, args[1]);
            if (!open.empty() && !close.empty()) {
                extCtx->charClassifier->addBracketPair(open[0], close[0]);
            }
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.chars.isWhitespace(char) -> bool
    // editor.chars.boslukMu(karakter) -> bool
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "isWhitespace"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->charClassifier || args.Length() < 1) return;
            std::string s = v8Str(iso, args[0]);
            if (s.empty()) return;
            args.GetReturnValue().Set(extCtx->charClassifier->isWhitespace(s[0]));
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.chars.isBracket(char) -> bool
    // editor.chars.parantezMi(karakter) -> bool
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "isBracket"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->charClassifier || args.Length() < 1) return;
            std::string s = v8Str(iso, args[0]);
            if (s.empty()) return;
            args.GetReturnValue().Set(extCtx->charClassifier->isBracket(s[0]));
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.chars.isOpenBracket(char) -> bool
    // editor.chars.acikParantezMi(karakter) -> bool
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "isOpenBracket"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->charClassifier || args.Length() < 1) return;
            std::string s = v8Str(iso, args[0]);
            if (s.empty()) return;
            args.GetReturnValue().Set(extCtx->charClassifier->isOpenBracket(s[0]));
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.chars.isCloseBracket(char) -> bool
    // editor.chars.kapaParantezMi(karakter) -> bool
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "isCloseBracket"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->charClassifier || args.Length() < 1) return;
            std::string s = v8Str(iso, args[0]);
            if (s.empty()) return;
            args.GetReturnValue().Set(extCtx->charClassifier->isCloseBracket(s[0]));
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.chars.matchingBracketChar(char) -> string (the matching bracket character)
    // editor.chars.eslesenParantezKar(karakter) -> string (eslesen parantez karakteri)
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "matchingBracketChar"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->charClassifier || args.Length() < 1) return;
            std::string s = v8Str(iso, args[0]);
            if (s.empty()) return;
            char m = extCtx->charClassifier->matchingBracket(s[0]);
            if (m == 0) {
                args.GetReturnValue().Set(v8::String::NewFromUtf8Literal(iso, ""));
                return;
            }
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(iso, std::string(1, m).c_str()).ToLocalChecked());
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.chars.removeWordChar(char) - remove an extra word character
    // editor.chars.kelimeKarKaldir(karakter) - ekstra kelime karakterini kaldir
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "removeWordChar"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->charClassifier || args.Length() < 1) return;
            std::string s = v8Str(iso, args[0]);
            if (!s.empty()) extCtx->charClassifier->removeWordChar(s[0]);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.chars.bracketPairs() -> [{open, close}, ...]
    // editor.chars.parantezCiftleri() -> [{ac, kapa}, ...]
    charsObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "bracketPairs"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->charClassifier) return;

            const auto& pairs = extCtx->charClassifier->bracketPairs();
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(pairs.size()));
            for (size_t i = 0; i < pairs.size(); ++i) {
                auto obj = v8::Object::New(iso);
                obj->Set(ctx2, v8::String::NewFromUtf8Literal(iso, "open"),
                    v8::String::NewFromUtf8(iso, std::string(1, pairs[i].open).c_str()).ToLocalChecked()).Check();
                obj->Set(ctx2, v8::String::NewFromUtf8Literal(iso, "close"),
                    v8::String::NewFromUtf8(iso, std::string(1, pairs[i].close).c_str()).ToLocalChecked()).Check();
                arr->Set(ctx2, static_cast<uint32_t>(i), obj).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
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
