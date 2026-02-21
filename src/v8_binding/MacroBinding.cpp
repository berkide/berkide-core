#include "MacroBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "MacroRecorder.h"
#include "commands.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Context for macro binding: holds MacroRecorder + CommandRouter for playback
// Makro binding baglami: oynatma icin MacroRecorder + CommandRouter tutar
struct MacroBindCtx {
    MacroRecorder* recorder;
    CommandRouter* router;
};

// Register editor.macro JS object
// editor.macro JS nesnesini kaydet
void RegisterMacroBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsMacro = v8::Object::New(isolate);

    auto* mctx = new MacroBindCtx{edCtx.macroRecorder, edCtx.commandRouter};

    // macro.record(register) - Start recording into a register
    // Bir register'a kaydetmeye basla
    jsMacro->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "record"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MacroBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->recorder || args.Length() < 1) return;
            std::string reg = v8Str(args.GetIsolate(), args[0]);
            mc->recorder->startRecording(reg);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // macro.stop() - Stop recording
    // Kaydi durdur
    jsMacro->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "stop"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MacroBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->recorder) return;
            mc->recorder->stopRecording();
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // macro.play(register, count?) - Play a macro
    // Bir makroyu oynat
    jsMacro->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "play"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MacroBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->recorder || !mc->router || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string reg = v8Str(iso, args[0]);
            int count = (args.Length() > 1) ? args[1]->Int32Value(ctx).FromJust() : 1;
            if (count < 1) count = 1;

            const auto* macro = mc->recorder->getMacro(reg);
            if (!macro) return;

            // Play the macro `count` times
            // Makroyu `count` kez oynat
            for (int i = 0; i < count; ++i) {
                for (auto& cmd : *macro) {
                    json cmdArgs = cmd.argsJson.empty() ? json::object() : json::parse(cmd.argsJson, nullptr, false);
                    if (cmdArgs.is_discarded()) cmdArgs = json::object();
                    mc->router->execute(cmd.name, cmdArgs);
                }
            }
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // macro.isRecording() -> bool
    // Kayit yapilip yapilmadigini kontrol et
    jsMacro->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isRecording"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MacroBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->recorder) { args.GetReturnValue().Set(false); return; }
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), mc->recorder->isRecording()));
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // macro.recordingRegister() -> string
    // Kayit yapilan register'in adini dondur
    jsMacro->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "recordingRegister"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MacroBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->recorder) return;
            auto* iso = args.GetIsolate();
            const std::string& reg = mc->recorder->recordingRegister();
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(iso, reg.c_str()).ToLocalChecked());
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // macro.list() -> [string, ...]  - List all register names with macros
    // Makro kayitli tum register adlarini listele
    jsMacro->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MacroBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->recorder) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            auto regs = mc->recorder->listRegisters();
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(regs.size()));
            for (size_t i = 0; i < regs.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i),
                    v8::String::NewFromUtf8(iso, regs[i].c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // macro.clear(register?) - Clear a register or all macros
    // Bir register'i veya tum makrolari temizle
    jsMacro->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clear"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MacroBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->recorder) return;
            if (args.Length() > 0) {
                std::string reg = v8Str(args.GetIsolate(), args[0]);
                mc->recorder->clearRegister(reg);
            } else {
                mc->recorder->clearAll();
            }
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
