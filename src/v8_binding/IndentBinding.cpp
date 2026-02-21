#include "IndentBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "buffers.h"
#include "IndentEngine.h"
#include <v8.h>

// Register editor.indent JS binding
// editor.indent JS binding'ini kaydet
void RegisterIndentBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto context = isolate->GetCurrentContext();
    auto indentObj = v8::Object::New(isolate);

    // editor.indent.config({useTabs, tabWidth, shiftWidth}) - get/set indent config
    // editor.indent.yapilandirma({sekmelerKullan, sekmeGenisligi, kaydirmaGenisligi}) - girinti yapilandirmasini al/ayarla
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "config"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->indentEngine) return;

            // If argument provided, set config
            // Arguman verilmisse, yapilandirmayi ayarla
            if (args.Length() > 0 && args[0]->IsObject()) {
                auto obj = args[0].As<v8::Object>();
                IndentConfig cfg = extCtx->indentEngine->config();

                auto useTabsKey = v8::String::NewFromUtf8Literal(iso, "useTabs");
                auto tabWidthKey = v8::String::NewFromUtf8Literal(iso, "tabWidth");
                auto shiftWidthKey = v8::String::NewFromUtf8Literal(iso, "shiftWidth");

                if (obj->Has(ctx2, useTabsKey).FromMaybe(false))
                    cfg.useTabs = obj->Get(ctx2, useTabsKey).ToLocalChecked()->BooleanValue(iso);
                if (obj->Has(ctx2, tabWidthKey).FromMaybe(false))
                    cfg.tabWidth = obj->Get(ctx2, tabWidthKey).ToLocalChecked()->Int32Value(ctx2).FromMaybe(4);
                if (obj->Has(ctx2, shiftWidthKey).FromMaybe(false))
                    cfg.shiftWidth = obj->Get(ctx2, shiftWidthKey).ToLocalChecked()->Int32Value(ctx2).FromMaybe(4);

                extCtx->indentEngine->setConfig(cfg);
            }

            // Return current config
            // Mevcut yapilandirmayi dondur
            const IndentConfig& cfg = extCtx->indentEngine->config();
            auto result = v8::Object::New(iso);
            result->Set(ctx2, v8::String::NewFromUtf8Literal(iso, "useTabs"),
                v8::Boolean::New(iso, cfg.useTabs)).Check();
            result->Set(ctx2, v8::String::NewFromUtf8Literal(iso, "tabWidth"),
                v8::Integer::New(iso, cfg.tabWidth)).Check();
            result->Set(ctx2, v8::String::NewFromUtf8Literal(iso, "shiftWidth"),
                v8::Integer::New(iso, cfg.shiftWidth)).Check();
            args.GetReturnValue().Set(result);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.indent.forNewLine(afterLine) -> {level, indentString}
    // editor.indent.yeniSatirIcin(sonrakiSatir) -> {seviye, girintiDizesi}
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "forNewLine"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->indentEngine || !extCtx->buffers || args.Length() < 1) return;

            int afterLine = args[0]->Int32Value(ctx2).FromMaybe(0);
            auto& buf = extCtx->buffers->active().getBuffer();
            IndentResult ir = extCtx->indentEngine->indentForNewLine(buf, afterLine);

            auto result = v8::Object::New(iso);
            result->Set(ctx2, v8::String::NewFromUtf8Literal(iso, "level"),
                v8::Integer::New(iso, ir.level)).Check();
            result->Set(ctx2, v8::String::NewFromUtf8Literal(iso, "indentString"),
                v8::String::NewFromUtf8(iso, ir.indentString.c_str()).ToLocalChecked()).Check();
            args.GetReturnValue().Set(result);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.indent.forLine(line) -> {level, indentString}
    // editor.indent.satirIcin(satir) -> {seviye, girintiDizesi}
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "forLine"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->indentEngine || !extCtx->buffers || args.Length() < 1) return;

            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            auto& buf = extCtx->buffers->active().getBuffer();
            IndentResult ir = extCtx->indentEngine->indentForLine(buf, line);

            auto result = v8::Object::New(iso);
            result->Set(ctx2, v8::String::NewFromUtf8Literal(iso, "level"),
                v8::Integer::New(iso, ir.level)).Check();
            result->Set(ctx2, v8::String::NewFromUtf8Literal(iso, "indentString"),
                v8::String::NewFromUtf8(iso, ir.indentString.c_str()).ToLocalChecked()).Check();
            args.GetReturnValue().Set(result);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.indent.getLevel(line) -> number
    // editor.indent.seviyeAl(satir) -> sayi
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "getLevel"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->indentEngine || !extCtx->buffers || args.Length() < 1) return;

            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            auto& buf = extCtx->buffers->active().getBuffer();
            if (line < 0 || line >= buf.lineCount()) return;

            int level = extCtx->indentEngine->getIndentLevel(buf.getLine(line));
            args.GetReturnValue().Set(level);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.indent.increase(line) - increase indent of a line
    // editor.indent.artir(satir) - bir satirin girintisini artir
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "increase"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->indentEngine || !extCtx->buffers || args.Length() < 1) return;

            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            auto& buf = extCtx->buffers->active().getBuffer();
            if (line < 0 || line >= buf.lineCount()) return;

            std::string newLine = extCtx->indentEngine->increaseIndent(buf.getLine(line));
            buf.getLineRef(line) = newLine;
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.indent.decrease(line) - decrease indent of a line
    // editor.indent.azalt(satir) - bir satirin girintisini azalt
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "decrease"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->indentEngine || !extCtx->buffers || args.Length() < 1) return;

            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            auto& buf = extCtx->buffers->active().getBuffer();
            if (line < 0 || line >= buf.lineCount()) return;

            std::string newLine = extCtx->indentEngine->decreaseIndent(buf.getLine(line));
            buf.getLineRef(line) = newLine;
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.indent.reindent(startLine, endLine) - reindent range
    // editor.indent.yenidenGirintile(baslangicSatir, bitisSatir) - araligi yeniden girintile
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "reindent"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->indentEngine || !extCtx->buffers || args.Length() < 2) return;

            int startLine = args[0]->Int32Value(ctx2).FromMaybe(0);
            int endLine   = args[1]->Int32Value(ctx2).FromMaybe(0);
            auto& buf = extCtx->buffers->active().getBuffer();
            extCtx->indentEngine->reindentRange(buf, startLine, endLine);
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.indent.makeIndentString(level) -> string
    // editor.indent.girintiDizesiOlustur(seviye) -> string
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "makeIndentString"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->indentEngine || args.Length() < 1) return;

            int level = args[0]->Int32Value(ctx2).FromMaybe(0);
            std::string result = extCtx->indentEngine->makeIndentString(level);
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(iso, result.c_str()).ToLocalChecked());
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.indent.getLeadingWhitespace(lineText) -> string
    // editor.indent.bastakiBoslukAl(satirMetni) -> string
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "getLeadingWhitespace"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->indentEngine || args.Length() < 1) return;

            v8::String::Utf8Value lineStr(iso, args[0]);
            std::string line = *lineStr ? *lineStr : "";
            std::string result = extCtx->indentEngine->getLeadingWhitespace(line);
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(iso, result.c_str()).ToLocalChecked());
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    // editor.indent.stripLeadingWhitespace(lineText) -> string
    // editor.indent.bastakiBoslukuCikar(satirMetni) -> string
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "stripLeadingWhitespace"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto extCtx = static_cast<EditorContext*>(args.Data().As<v8::External>()->Value());
            if (!extCtx->indentEngine || args.Length() < 1) return;

            v8::String::Utf8Value lineStr(iso, args[0]);
            std::string line = *lineStr ? *lineStr : "";
            std::string result = extCtx->indentEngine->stripLeadingWhitespace(line);
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(iso, result.c_str()).ToLocalChecked());
        }, v8::External::New(isolate, &ctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "indent"),
        indentObj).Check();
}

// Self-register at static initialization time
// Statik baslatma zamaninda kendini kaydet
static bool _indentReg = [] {
    BindingRegistry::instance().registerBinding("indent", RegisterIndentBinding);
    return true;
}();
