// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "IndentBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "buffers.h"
#include "IndentEngine.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Context for indent binding
// Girinti binding baglami
struct IndentCtx {
    EditorContext* edCtx;
    I18n* i18n;
};

// Register editor.indent JS binding with standard response format
// Standart yanit formatiyla editor.indent JS binding'ini kaydet
void RegisterIndentBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto context = isolate->GetCurrentContext();
    auto indentObj = v8::Object::New(isolate);

    auto* ictx = new IndentCtx{&ctx, ctx.i18n};

    // editor.indent.config({useTabs, tabWidth, shiftWidth}) -> {ok, data: {useTabs, tabWidth, shiftWidth}}
    // Girinti yapilandirmasini al/ayarla
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "config"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ic = static_cast<IndentCtx*>(args.Data().As<v8::External>()->Value());
            if (!ic || !ic->edCtx || !ic->edCtx->indentEngine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "indentEngine"}}, ic ? ic->i18n : nullptr);
                return;
            }
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();

            // If argument provided, set config
            // Arguman verilmisse, yapilandirmayi ayarla
            if (args.Length() > 0 && args[0]->IsObject()) {
                auto obj = args[0].As<v8::Object>();
                IndentConfig cfg = ic->edCtx->indentEngine->config();

                auto useTabsKey = v8::String::NewFromUtf8Literal(iso, "useTabs");
                auto tabWidthKey = v8::String::NewFromUtf8Literal(iso, "tabWidth");
                auto shiftWidthKey = v8::String::NewFromUtf8Literal(iso, "shiftWidth");

                if (obj->Has(ctx2, useTabsKey).FromMaybe(false))
                    cfg.useTabs = obj->Get(ctx2, useTabsKey).ToLocalChecked()->BooleanValue(iso);
                if (obj->Has(ctx2, tabWidthKey).FromMaybe(false))
                    cfg.tabWidth = obj->Get(ctx2, tabWidthKey).ToLocalChecked()->Int32Value(ctx2).FromMaybe(4);
                if (obj->Has(ctx2, shiftWidthKey).FromMaybe(false))
                    cfg.shiftWidth = obj->Get(ctx2, shiftWidthKey).ToLocalChecked()->Int32Value(ctx2).FromMaybe(4);

                ic->edCtx->indentEngine->setConfig(cfg);
            }

            // Return current config
            // Mevcut yapilandirmayi dondur
            const IndentConfig& cfg = ic->edCtx->indentEngine->config();
            json data = {
                {"useTabs", cfg.useTabs},
                {"tabWidth", cfg.tabWidth},
                {"shiftWidth", cfg.shiftWidth}
            };
            V8Response::ok(args, data);
        }, v8::External::New(isolate, ictx)).ToLocalChecked()
    ).Check();

    // editor.indent.forNewLine(afterLine) -> {ok, data: {level, indentString}}
    // Yeni satir icin girinti hesapla
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "forNewLine"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ic = static_cast<IndentCtx*>(args.Data().As<v8::External>()->Value());
            if (!ic || !ic->edCtx || !ic->edCtx->indentEngine || !ic->edCtx->buffers) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "indentEngine"}}, ic ? ic->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "afterLine"}}, ic->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            int afterLine = args[0]->Int32Value(ctx2).FromMaybe(0);
            auto& buf = ic->edCtx->buffers->active().getBuffer();
            IndentResult ir = ic->edCtx->indentEngine->indentForNewLine(buf, afterLine);

            json data = {
                {"level", ir.level},
                {"indentString", ir.indentString}
            };
            V8Response::ok(args, data);
        }, v8::External::New(isolate, ictx)).ToLocalChecked()
    ).Check();

    // editor.indent.forLine(line) -> {ok, data: {level, indentString}}
    // Satir icin girinti hesapla
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "forLine"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ic = static_cast<IndentCtx*>(args.Data().As<v8::External>()->Value());
            if (!ic || !ic->edCtx || !ic->edCtx->indentEngine || !ic->edCtx->buffers) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "indentEngine"}}, ic ? ic->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "line"}}, ic->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            auto& buf = ic->edCtx->buffers->active().getBuffer();
            IndentResult ir = ic->edCtx->indentEngine->indentForLine(buf, line);

            json data = {
                {"level", ir.level},
                {"indentString", ir.indentString}
            };
            V8Response::ok(args, data);
        }, v8::External::New(isolate, ictx)).ToLocalChecked()
    ).Check();

    // editor.indent.getLevel(line) -> {ok, data: number}
    // Satirin girinti seviyesini al
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "getLevel"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ic = static_cast<IndentCtx*>(args.Data().As<v8::External>()->Value());
            if (!ic || !ic->edCtx || !ic->edCtx->indentEngine || !ic->edCtx->buffers) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "indentEngine"}}, ic ? ic->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "line"}}, ic->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            auto& buf = ic->edCtx->buffers->active().getBuffer();
            if (line < 0 || line >= buf.lineCount()) {
                V8Response::error(args, "OUT_OF_RANGE", "args.out_of_range",
                    {{"name", "line"}}, ic->i18n);
                return;
            }

            int level = ic->edCtx->indentEngine->getIndentLevel(buf.getLine(line));
            V8Response::ok(args, level);
        }, v8::External::New(isolate, ictx)).ToLocalChecked()
    ).Check();

    // editor.indent.increase(line) -> {ok, data: true}
    // Bir satirin girintisini artir
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "increase"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ic = static_cast<IndentCtx*>(args.Data().As<v8::External>()->Value());
            if (!ic || !ic->edCtx || !ic->edCtx->indentEngine || !ic->edCtx->buffers) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "indentEngine"}}, ic ? ic->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "line"}}, ic->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            auto& buf = ic->edCtx->buffers->active().getBuffer();
            if (line < 0 || line >= buf.lineCount()) {
                V8Response::error(args, "OUT_OF_RANGE", "args.out_of_range",
                    {{"name", "line"}}, ic->i18n);
                return;
            }

            std::string newLine = ic->edCtx->indentEngine->increaseIndent(buf.getLine(line));
            buf.getLineRef(line) = newLine;
            V8Response::ok(args, true);
        }, v8::External::New(isolate, ictx)).ToLocalChecked()
    ).Check();

    // editor.indent.decrease(line) -> {ok, data: true}
    // Bir satirin girintisini azalt
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "decrease"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ic = static_cast<IndentCtx*>(args.Data().As<v8::External>()->Value());
            if (!ic || !ic->edCtx || !ic->edCtx->indentEngine || !ic->edCtx->buffers) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "indentEngine"}}, ic ? ic->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "line"}}, ic->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            int line = args[0]->Int32Value(ctx2).FromMaybe(0);
            auto& buf = ic->edCtx->buffers->active().getBuffer();
            if (line < 0 || line >= buf.lineCount()) {
                V8Response::error(args, "OUT_OF_RANGE", "args.out_of_range",
                    {{"name", "line"}}, ic->i18n);
                return;
            }

            std::string newLine = ic->edCtx->indentEngine->decreaseIndent(buf.getLine(line));
            buf.getLineRef(line) = newLine;
            V8Response::ok(args, true);
        }, v8::External::New(isolate, ictx)).ToLocalChecked()
    ).Check();

    // editor.indent.reindent(startLine, endLine) -> {ok, data: true}
    // Araligi yeniden girintile
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "reindent"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ic = static_cast<IndentCtx*>(args.Data().As<v8::External>()->Value());
            if (!ic || !ic->edCtx || !ic->edCtx->indentEngine || !ic->edCtx->buffers) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "indentEngine"}}, ic ? ic->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "startLine, endLine"}}, ic->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            int startLine = args[0]->Int32Value(ctx2).FromMaybe(0);
            int endLine   = args[1]->Int32Value(ctx2).FromMaybe(0);
            auto& buf = ic->edCtx->buffers->active().getBuffer();
            ic->edCtx->indentEngine->reindentRange(buf, startLine, endLine);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, ictx)).ToLocalChecked()
    ).Check();

    // editor.indent.makeIndentString(level) -> {ok, data: string}
    // Girinti dizesi olustur
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "makeIndentString"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ic = static_cast<IndentCtx*>(args.Data().As<v8::External>()->Value());
            if (!ic || !ic->edCtx || !ic->edCtx->indentEngine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "indentEngine"}}, ic ? ic->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "level"}}, ic->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            auto ctx2 = iso->GetCurrentContext();
            int level = args[0]->Int32Value(ctx2).FromMaybe(0);
            std::string result = ic->edCtx->indentEngine->makeIndentString(level);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, ictx)).ToLocalChecked()
    ).Check();

    // editor.indent.getLeadingWhitespace(lineText) -> {ok, data: string}
    // Bastaki boslugu al
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "getLeadingWhitespace"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ic = static_cast<IndentCtx*>(args.Data().As<v8::External>()->Value());
            if (!ic || !ic->edCtx || !ic->edCtx->indentEngine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "indentEngine"}}, ic ? ic->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "lineText"}}, ic->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            std::string line = v8Str(iso, args[0]);
            std::string result = ic->edCtx->indentEngine->getLeadingWhitespace(line);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, ictx)).ToLocalChecked()
    ).Check();

    // editor.indent.stripLeadingWhitespace(lineText) -> {ok, data: string}
    // Bastaki boslugu cikar
    indentObj->Set(context,
        v8::String::NewFromUtf8Literal(isolate, "stripLeadingWhitespace"),
        v8::Function::New(context, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ic = static_cast<IndentCtx*>(args.Data().As<v8::External>()->Value());
            if (!ic || !ic->edCtx || !ic->edCtx->indentEngine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "indentEngine"}}, ic ? ic->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "lineText"}}, ic->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            std::string line = v8Str(iso, args[0]);
            std::string result = ic->edCtx->indentEngine->stripLeadingWhitespace(line);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, ictx)).ToLocalChecked()
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
