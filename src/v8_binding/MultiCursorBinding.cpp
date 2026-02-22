// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "MultiCursorBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "MultiCursor.h"
#include "buffers.h"
#include "state.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Context for multi-cursor binding
// Coklu imlec binding baglami
struct MCBindCtx {
    MultiCursor* mc;
    Buffers* bufs;
    I18n* i18n;
};

// Helper: convert CursorEntry to nlohmann::json
// Yardimci: CursorEntry'yi nlohmann::json'a cevir
static json cursorToJson(const CursorEntry& c) {
    json obj = {
        {"line", c.line},
        {"col", c.col},
        {"hasSelection", c.hasSelection}
    };
    if (c.hasSelection) {
        obj["anchorLine"] = c.anchorLine;
        obj["anchorCol"] = c.anchorCol;
    }
    return obj;
}

// Register editor.multicursor JS object with standard response format
// Standart yanit formatiyla editor.multicursor JS nesnesini kaydet
void RegisterMultiCursorBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsMC = v8::Object::New(isolate);

    auto* mctx = new MCBindCtx{edCtx.multiCursor, edCtx.buffers, edCtx.i18n};

    // multicursor.add(line, col) -> {ok, data: index}
    // Konuma yeni imlec ekle
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "add"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "line, col"}}, mc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            int line = args[0]->Int32Value(ctx).FromJust();
            int col  = args[1]->Int32Value(ctx).FromJust();
            int idx = mc->mc->addCursor(line, col);
            V8Response::ok(args, idx);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.remove(index) -> {ok, data: bool}
    // Dizine gore imleci kaldir
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "remove"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "index"}}, mc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int idx = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            bool removed = mc->mc->removeCursor(idx);
            V8Response::ok(args, removed);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.clear() -> {ok, data: true}
    // Tum ikincil imlecleri temizle
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clear"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            mc->mc->clearSecondary();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.list() -> {ok, data: [cursor, ...], meta: {total: N}}
    // Tum imlecleri listele
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }

            auto& cursors = mc->mc->cursors();
            json arr = json::array();
            for (const auto& c : cursors) {
                arr.push_back(cursorToJson(c));
            }
            json meta = {{"total", cursors.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.count() -> {ok, data: number}
    // Imlec sayisini al
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "count"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            V8Response::ok(args, mc->mc->count());
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.isActive() -> {ok, data: bool}
    // Coklu imlec modunun aktif olup olmadigini kontrol et
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isActive"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            V8Response::ok(args, mc->mc->isActive());
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.insertAll(text) -> {ok, data: true}
    // Tum imleclere metin ekle
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "insertAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "text"}}, mc->i18n);
                return;
            }
            std::string text = v8Str(args.GetIsolate(), args[0]);
            mc->mc->insertAtAll(mc->bufs->active().getBuffer(), text);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.addNextMatch(word) -> {ok, data: index}
    // Kelimenin sonraki olusumuna imlec ekle
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "addNextMatch"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "word"}}, mc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            std::string word = v8Str(iso, args[0]);
            int idx = mc->mc->addCursorAtNextMatch(mc->bufs->active().getBuffer(), word);
            V8Response::ok(args, idx);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.addOnLines(startLine, endLine, col) -> {ok, data: true}
    // Satir araligina imlecler ekle
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "addOnLines"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            if (args.Length() < 3) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "startLine, endLine, col"}}, mc->i18n);
                return;
            }
            auto ctx = args.GetIsolate()->GetCurrentContext();
            int startLine = args[0]->Int32Value(ctx).FromJust();
            int endLine   = args[1]->Int32Value(ctx).FromJust();
            int col       = args[2]->Int32Value(ctx).FromJust();
            mc->mc->addCursorsOnLines(startLine, endLine, col);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.setPrimary(line, col) -> {ok, data: true}
    // Birincil imlec konumunu ayarla
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setPrimary"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "line, col"}}, mc->i18n);
                return;
            }
            auto ctx = args.GetIsolate()->GetCurrentContext();
            int line = args[0]->Int32Value(ctx).FromJust();
            int col  = args[1]->Int32Value(ctx).FromJust();
            mc->mc->setPrimary(line, col);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.primary() -> {ok, data: {line, col, hasSelection, ...}}
    // Birincil imleci al
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "primary"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            V8Response::ok(args, cursorToJson(mc->mc->primary()));
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.moveAllUp() -> {ok, data: true}
    // Tum imlecleri yukari tasi
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveAllUp"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            mc->mc->moveAllUp(mc->bufs->active().getBuffer());
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.moveAllDown() -> {ok, data: true}
    // Tum imlecleri asagi tasi
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveAllDown"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            mc->mc->moveAllDown(mc->bufs->active().getBuffer());
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.moveAllLeft() -> {ok, data: true}
    // Tum imlecleri sola tasi
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveAllLeft"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            mc->mc->moveAllLeft(mc->bufs->active().getBuffer());
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.moveAllRight() -> {ok, data: true}
    // Tum imlecleri saga tasi
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveAllRight"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            mc->mc->moveAllRight(mc->bufs->active().getBuffer());
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.moveAllToLineStart() -> {ok, data: true}
    // Tum imlecleri satir basina tasi
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveAllToLineStart"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            mc->mc->moveAllToLineStart();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.moveAllToLineEnd() -> {ok, data: true}
    // Tum imlecleri satir sonuna tasi
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveAllToLineEnd"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            mc->mc->moveAllToLineEnd(mc->bufs->active().getBuffer());
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.backspaceAtAll() -> {ok, data: true}
    // Tum imleclerden onceki karakteri sil
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "backspaceAtAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            mc->mc->backspaceAtAll(mc->bufs->active().getBuffer());
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.deleteAtAll() -> {ok, data: true}
    // Tum imleclerdeki karakteri sil
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "deleteAtAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            mc->mc->deleteAtAll(mc->bufs->active().getBuffer());
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.setAnchorAtAll() -> {ok, data: true}
    // Tum imleclerde secim baglama noktasi ayarla
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setAnchorAtAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            mc->mc->setAnchorAtAll();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.clearSelectionAtAll() -> {ok, data: true}
    // Tum imleclerde secimi temizle
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clearSelectionAtAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            mc->mc->clearSelectionAtAll();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.dedup() -> {ok, data: true}
    // Ayni konumdaki tekrar eden imlecleri kaldir
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "dedup"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            mc->mc->dedup();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.sort() -> {ok, data: true}
    // Imlecleri konuma gore sirala
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "sort"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "multiCursor"}}, mc ? mc->i18n : nullptr);
                return;
            }
            mc->mc->sort();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "multicursor"),
        jsMC).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _mcReg = [] {
    BindingRegistry::instance().registerBinding("multicursor", RegisterMultiCursorBinding);
    return true;
}();
