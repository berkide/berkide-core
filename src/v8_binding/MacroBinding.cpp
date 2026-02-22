// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "MacroBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "MacroRecorder.h"
#include "commands.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Context for macro binding: holds MacroRecorder + CommandRouter + I18n for playback
// Makro binding baglami: oynatma icin MacroRecorder + CommandRouter + I18n tutar
struct MacroBindCtx {
    MacroRecorder* recorder;
    CommandRouter* router;
    I18n* i18n;
};

// Register editor.macro JS object
// editor.macro JS nesnesini kaydet
void RegisterMacroBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsMacro = v8::Object::New(isolate);

    auto* mctx = new MacroBindCtx{edCtx.macroRecorder, edCtx.commandRouter, edCtx.i18n};

    // macro.record(register) -> {ok, data: true, ...} - Start recording into a register
    // Bir register'a kaydetmeye basla
    jsMacro->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "record"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MacroBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->recorder) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, mc ? mc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "register"}}, mc->i18n);
                return;
            }
            std::string reg = v8Str(args.GetIsolate(), args[0]);
            mc->recorder->startRecording(reg);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // macro.stop() -> {ok, data: true, ...} - Stop recording
    // Kaydi durdur
    jsMacro->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "stop"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MacroBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->recorder) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, mc ? mc->i18n : nullptr);
                return;
            }
            mc->recorder->stopRecording();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // macro.play(register, count?) -> {ok, data: true, ...} - Play a macro
    // Bir makroyu oynat
    jsMacro->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "play"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MacroBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->recorder || !mc->router) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, mc ? mc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "register"}}, mc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string reg = v8Str(iso, args[0]);
            int count = (args.Length() > 1) ? args[1]->Int32Value(ctx).FromJust() : 1;
            if (count < 1) count = 1;

            const auto* macro = mc->recorder->getMacro(reg);
            if (!macro) {
                V8Response::error(args, "MACRO_NOT_FOUND", "macro.not_found",
                    {{"register", reg}}, mc->i18n);
                return;
            }

            // Play the macro `count` times
            // Makroyu `count` kez oynat
            for (int i = 0; i < count; ++i) {
                for (auto& cmd : *macro) {
                    json cmdArgs = cmd.argsJson.empty() ? json::object() : json::parse(cmd.argsJson, nullptr, false);
                    if (cmdArgs.is_discarded()) cmdArgs = json::object();
                    mc->router->execute(cmd.name, cmdArgs);
                }
            }
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // macro.isRecording() -> {ok, data: bool, ...}
    // Kayit yapilip yapilmadigini kontrol et
    jsMacro->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isRecording"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MacroBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->recorder) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, mc ? mc->i18n : nullptr);
                return;
            }
            bool recording = mc->recorder->isRecording();
            V8Response::ok(args, recording);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // macro.recordingRegister() -> {ok, data: "registerName", ...}
    // Kayit yapilan register'in adini dondur
    jsMacro->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "recordingRegister"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MacroBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->recorder) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, mc ? mc->i18n : nullptr);
                return;
            }
            std::string reg = mc->recorder->recordingRegister();
            V8Response::ok(args, reg);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // macro.list() -> {ok, data: ["a", "b", ...], meta: {total: N}, ...} - List all register names with macros
    // Makro kayitli tum register adlarini listele
    jsMacro->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MacroBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->recorder) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, mc ? mc->i18n : nullptr);
                return;
            }

            auto regs = mc->recorder->listRegisters();
            json arr = json::array();
            for (size_t i = 0; i < regs.size(); ++i) {
                arr.push_back(regs[i]);
            }
            json meta = {{"total", regs.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // macro.clear(register?) -> {ok, data: true, ...} - Clear a register or all macros
    // Bir register'i veya tum makrolari temizle
    jsMacro->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clear"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MacroBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->recorder) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, mc ? mc->i18n : nullptr);
                return;
            }
            if (args.Length() > 0) {
                std::string reg = v8Str(args.GetIsolate(), args[0]);
                mc->recorder->clearRegister(reg);
            } else {
                mc->recorder->clearAll();
            }
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "macro"),
        jsMacro).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _macroReg = [] {
    BindingRegistry::instance().registerBinding("macro", RegisterMacroBinding);
    return true;
}();
