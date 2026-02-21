#include "SelectionBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "buffers.h"
#include "state.h"
#include "Selection.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Register editor.selection JS object with get, set, clear, getText, getRange, setType
// editor.selection JS nesnesini get, set, clear, getText, getRange, setType ile kaydet
void RegisterSelectionBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsSel = v8::Object::New(isolate);
    Buffers* bufs = ctx.buffers;

    // selection.setAnchor(line, col) - Start or update selection anchor
    // Secim baglama noktasini ayarla veya guncelle
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setAnchor"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            if (!bufs) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            auto& st = bufs->active();

            int line = (args.Length() > 0) ? args[0]->Int32Value(ctx).FromMaybe(0) : st.getCursor().getLine();
            int col  = (args.Length() > 1) ? args[1]->Int32Value(ctx).FromMaybe(0) : st.getCursor().getCol();
            st.getSelection().setAnchor(line, col);
        }, v8::External::New(isolate, bufs)).ToLocalChecked()
    ).Check();

    // selection.clear() - Deactivate selection
    // Secimi temizle (devre disi birak)
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clear"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            if (!bufs) return;
            bufs->active().getSelection().clear();
        }, v8::External::New(isolate, bufs)).ToLocalChecked()
    ).Check();

    // selection.isActive() -> bool
    // Secimin etkin olup olmadigini kontrol et
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isActive"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            if (!bufs) return;
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(),
                bufs->active().getSelection().isActive()));
        }, v8::External::New(isolate, bufs)).ToLocalChecked()
    ).Check();

    // selection.getText() -> string - Get selected text content
    // Secili metin icerigini al
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getText"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            if (!bufs) return;
            auto* iso = args.GetIsolate();
            auto& st = bufs->active();
            auto& sel = st.getSelection();
            if (!sel.isActive()) {
                args.GetReturnValue().Set(v8::String::NewFromUtf8Literal(iso, ""));
                return;
            }
            std::string text = sel.getText(st.getBuffer(),
                                           st.getCursor().getLine(),
                                           st.getCursor().getCol());
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(iso, text.c_str()).ToLocalChecked());
        }, v8::External::New(isolate, bufs)).ToLocalChecked()
    ).Check();

    // selection.getRange() -> {startLine, startCol, endLine, endCol} | null
    // Secim araligini al veya secim yoksa null dondur
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getRange"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            if (!bufs) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            auto& st = bufs->active();
            auto& sel = st.getSelection();

            if (!sel.isActive()) {
                args.GetReturnValue().SetNull();
                return;
            }

            int sLine, sCol, eLine, eCol;
            sel.getRange(st.getCursor().getLine(), st.getCursor().getCol(),
                         sLine, sCol, eLine, eCol);

            v8::Local<v8::Object> obj = v8::Object::New(iso);
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "startLine"),
                v8::Integer::New(iso, sLine)).Check();
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "startCol"),
                v8::Integer::New(iso, sCol)).Check();
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "endLine"),
                v8::Integer::New(iso, eLine)).Check();
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "endCol"),
                v8::Integer::New(iso, eCol)).Check();
            args.GetReturnValue().Set(obj);
        }, v8::External::New(isolate, bufs)).ToLocalChecked()
    ).Check();

    // selection.setType(type) - Set selection type: "char", "line", "block"
    // Secim turunu ayarla: "char", "line", "block"
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setType"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            if (!bufs || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            std::string typeStr;
            v8::String::Utf8Value s(iso, args[0]);
            if (*s) typeStr = *s;

            auto& sel = bufs->active().getSelection();
            if (typeStr == "line")       sel.setType(SelectionType::Line);
            else if (typeStr == "block") sel.setType(SelectionType::Block);
            else                         sel.setType(SelectionType::Char);
        }, v8::External::New(isolate, bufs)).ToLocalChecked()
    ).Check();

    // selection.getType() -> "char" | "line" | "block"
    // Secim turunu al
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getType"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            if (!bufs) return;
            auto* iso = args.GetIsolate();
            auto type = bufs->active().getSelection().type();
            const char* str = (type == SelectionType::Line)  ? "line" :
                              (type == SelectionType::Block) ? "block" : "char";
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(iso, str).ToLocalChecked());
        }, v8::External::New(isolate, bufs)).ToLocalChecked()
    ).Check();

    // selection.anchorLine() -> int - Get selection anchor line
    // Secim baglama satir numarasini al
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "anchorLine"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            if (!bufs) return;
            args.GetReturnValue().Set(bufs->active().getSelection().anchorLine());
        }, v8::External::New(isolate, bufs)).ToLocalChecked()
    ).Check();

    // selection.anchorCol() -> int - Get selection anchor column
    // Secim baglama sutun numarasini al
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "anchorCol"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* bufs = static_cast<Buffers*>(args.Data().As<v8::External>()->Value());
            if (!bufs) return;
            args.GetReturnValue().Set(bufs->active().getSelection().anchorCol());
        }, v8::External::New(isolate, bufs)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "selection"),
        jsSel).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _selectionReg = [] {
    BindingRegistry::instance().registerBinding("selection", RegisterSelectionBinding);
    return true;
}();
