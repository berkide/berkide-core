// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "SelectionBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "buffers.h"
#include "state.h"
#include "Selection.h"
#include <v8.h>

// Context struct for selection binding lambdas
// Secim binding lambda'lari icin baglam yapisi
struct SelectionCtx {
    Buffers* bufs;
    I18n* i18n;
};

// Register editor.selection JS object with get, set, clear, getText, getRange, setType
// editor.selection JS nesnesini get, set, clear, getText, getRange, setType ile kaydet
void RegisterSelectionBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsSel = v8::Object::New(isolate);

    auto* sctx = new SelectionCtx{ctx.buffers, ctx.i18n};

    // selection.setAnchor(line, col) - Start or update selection anchor
    // Secim baglama noktasini ayarla veya guncelle
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setAnchor"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SelectionCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, sc ? sc->i18n : nullptr);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            auto& st = sc->bufs->active();

            int line = (args.Length() > 0) ? args[0]->Int32Value(ctx).FromMaybe(0) : st.getCursor().getLine();
            int col  = (args.Length() > 1) ? args[1]->Int32Value(ctx).FromMaybe(0) : st.getCursor().getCol();
            st.getSelection().setAnchor(line, col);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // selection.clear() - Deactivate selection
    // Secimi temizle (devre disi birak)
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clear"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SelectionCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, sc ? sc->i18n : nullptr);
                return;
            }
            sc->bufs->active().getSelection().clear();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // selection.isActive() -> {ok, data: bool, ...}
    // Secimin etkin olup olmadigini kontrol et
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isActive"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SelectionCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, sc ? sc->i18n : nullptr);
                return;
            }
            bool active = sc->bufs->active().getSelection().isActive();
            V8Response::ok(args, active);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // selection.getText() -> {ok, data: string, ...} - Get selected text content
    // Secili metin icerigini al
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getText"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SelectionCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, sc ? sc->i18n : nullptr);
                return;
            }
            auto& st = sc->bufs->active();
            auto& sel = st.getSelection();
            if (!sel.isActive()) {
                V8Response::ok(args, std::string(""));
                return;
            }
            std::string text = sel.getText(st.getBuffer(),
                                           st.getCursor().getLine(),
                                           st.getCursor().getCol());
            V8Response::ok(args, text);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // selection.getRange() -> {ok, data: {startLine, startCol, endLine, endCol} | null, ...}
    // Secim araligini al veya secim yoksa null dondur
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getRange"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SelectionCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, sc ? sc->i18n : nullptr);
                return;
            }
            auto& st = sc->bufs->active();
            auto& sel = st.getSelection();

            if (!sel.isActive()) {
                V8Response::ok(args, nullptr);
                return;
            }

            int sLine, sCol, eLine, eCol;
            sel.getRange(st.getCursor().getLine(), st.getCursor().getCol(),
                         sLine, sCol, eLine, eCol);

            json data = {
                {"startLine", sLine},
                {"startCol", sCol},
                {"endLine", eLine},
                {"endCol", eCol}
            };
            V8Response::ok(args, data);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // selection.setType(type) - Set selection type: "char", "line", "block"
    // Secim turunu ayarla: "char", "line", "block"
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setType"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SelectionCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, sc ? sc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "type"}}, sc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            v8::String::Utf8Value s(iso, args[0]);
            std::string typeStr = *s ? *s : "";

            auto& sel = sc->bufs->active().getSelection();
            if (typeStr == "line")       sel.setType(SelectionType::Line);
            else if (typeStr == "block") sel.setType(SelectionType::Block);
            else                         sel.setType(SelectionType::Char);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // selection.getType() -> {ok, data: "char" | "line" | "block", ...}
    // Secim turunu al
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getType"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SelectionCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, sc ? sc->i18n : nullptr);
                return;
            }
            auto type = sc->bufs->active().getSelection().type();
            std::string str = (type == SelectionType::Line)  ? "line" :
                              (type == SelectionType::Block) ? "block" : "char";
            V8Response::ok(args, str);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // selection.anchorLine() -> {ok, data: int, ...} - Get selection anchor line
    // Secim baglama satir numarasini al
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "anchorLine"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SelectionCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, sc ? sc->i18n : nullptr);
                return;
            }
            int line = sc->bufs->active().getSelection().anchorLine();
            V8Response::ok(args, line);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
    ).Check();

    // selection.anchorCol() -> {ok, data: int, ...} - Get selection anchor column
    // Secim baglama sutun numarasini al
    jsSel->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "anchorCol"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* sc = static_cast<SelectionCtx*>(args.Data().As<v8::External>()->Value());
            if (!sc || !sc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "buffers"}}, sc ? sc->i18n : nullptr);
                return;
            }
            int col = sc->bufs->active().getSelection().anchorCol();
            V8Response::ok(args, col);
        }, v8::External::New(isolate, sctx)).ToLocalChecked()
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
