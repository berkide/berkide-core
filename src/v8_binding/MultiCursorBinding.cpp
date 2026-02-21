#include "MultiCursorBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
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
};

// Helper: convert CursorEntry to V8 object
// Yardimci: CursorEntry'yi V8 nesnesine cevir
static v8::Local<v8::Object> cursorToV8(v8::Isolate* iso, v8::Local<v8::Context> ctx,
                                          const CursorEntry& c) {
    v8::Local<v8::Object> obj = v8::Object::New(iso);
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "line"),
        v8::Integer::New(iso, c.line)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "col"),
        v8::Integer::New(iso, c.col)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "hasSelection"),
        v8::Boolean::New(iso, c.hasSelection)).Check();
    if (c.hasSelection) {
        obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "anchorLine"),
            v8::Integer::New(iso, c.anchorLine)).Check();
        obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "anchorCol"),
            v8::Integer::New(iso, c.anchorCol)).Check();
    }
    return obj;
}

// Register editor.multicursor JS object
// editor.multicursor JS nesnesini kaydet
void RegisterMultiCursorBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsMC = v8::Object::New(isolate);

    auto* mctx = new MCBindCtx{edCtx.multiCursor, edCtx.buffers};

    // multicursor.add(line, col) -> index
    // Konuma yeni imlec ekle
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "add"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            int line = args[0]->Int32Value(ctx).FromJust();
            int col  = args[1]->Int32Value(ctx).FromJust();
            int idx = mc->mc->addCursor(line, col);
            args.GetReturnValue().Set(v8::Integer::New(iso, idx));
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.remove(index) -> bool
    // Dizine gore imleci kaldir
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "remove"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            int idx = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            args.GetReturnValue().Set(v8::Boolean::New(iso, mc->mc->removeCursor(idx)));
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.clear() - Clear all secondary cursors
    // Tum ikincil imlecleri temizle
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clear"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) return;
            mc->mc->clearSecondary();
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.list() -> [cursor, ...]
    // Tum imlecleri listele
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            auto& cursors = mc->mc->cursors();
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(cursors.size()));
            for (size_t i = 0; i < cursors.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i), cursorToV8(iso, ctx, cursors[i])).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.count() -> number
    // Imlec sayisini al
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "count"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) return;
            args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), mc->mc->count()));
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.isActive() -> bool
    // Coklu imlec modunun aktif olup olmadigini kontrol et
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isActive"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) { args.GetReturnValue().Set(false); return; }
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), mc->mc->isActive()));
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.insertAll(text) - Insert text at all cursors
    // Tum imleclere metin ekle
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "insertAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs || args.Length() < 1) return;
            std::string text = v8Str(args.GetIsolate(), args[0]);
            mc->mc->insertAtAll(mc->bufs->active().getBuffer(), text);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.addNextMatch(word) -> index
    // Kelimenin sonraki olusumuna imlec ekle
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "addNextMatch"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            std::string word = v8Str(iso, args[0]);
            int idx = mc->mc->addCursorAtNextMatch(mc->bufs->active().getBuffer(), word);
            args.GetReturnValue().Set(v8::Integer::New(iso, idx));
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.addOnLines(startLine, endLine, col) - Add cursors on line range
    // Satir araligina imlecler ekle
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "addOnLines"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || args.Length() < 3) return;
            auto ctx = args.GetIsolate()->GetCurrentContext();
            int startLine = args[0]->Int32Value(ctx).FromJust();
            int endLine   = args[1]->Int32Value(ctx).FromJust();
            int col       = args[2]->Int32Value(ctx).FromJust();
            mc->mc->addCursorsOnLines(startLine, endLine, col);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.setPrimary(line, col) - Set primary cursor position
    // Birincil imlec konumunu ayarla
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setPrimary"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || args.Length() < 2) return;
            auto ctx = args.GetIsolate()->GetCurrentContext();
            int line = args[0]->Int32Value(ctx).FromJust();
            int col  = args[1]->Int32Value(ctx).FromJust();
            mc->mc->setPrimary(line, col);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.primary() -> {line, col, hasSelection, ...}
    // Birincil imleci al
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "primary"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            args.GetReturnValue().Set(cursorToV8(iso, ctx, mc->mc->primary()));
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.moveAllUp() - Move all cursors up
    // Tum imlecleri yukari tasi
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveAllUp"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs) return;
            mc->mc->moveAllUp(mc->bufs->active().getBuffer());
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.moveAllDown() - Move all cursors down
    // Tum imlecleri asagi tasi
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveAllDown"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs) return;
            mc->mc->moveAllDown(mc->bufs->active().getBuffer());
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.moveAllLeft() - Move all cursors left
    // Tum imlecleri sola tasi
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveAllLeft"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs) return;
            mc->mc->moveAllLeft(mc->bufs->active().getBuffer());
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.moveAllRight() - Move all cursors right
    // Tum imlecleri saga tasi
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveAllRight"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs) return;
            mc->mc->moveAllRight(mc->bufs->active().getBuffer());
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.moveAllToLineStart() - Move all cursors to line start
    // Tum imlecleri satir basina tasi
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveAllToLineStart"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) return;
            mc->mc->moveAllToLineStart();
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.moveAllToLineEnd() - Move all cursors to line end
    // Tum imlecleri satir sonuna tasi
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "moveAllToLineEnd"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs) return;
            mc->mc->moveAllToLineEnd(mc->bufs->active().getBuffer());
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.backspaceAtAll() - Delete char before all cursors
    // Tum imleclerden onceki karakteri sil
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "backspaceAtAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs) return;
            mc->mc->backspaceAtAll(mc->bufs->active().getBuffer());
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.deleteAtAll() - Delete char at all cursors
    // Tum imleclerdeki karakteri sil
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "deleteAtAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc || !mc->bufs) return;
            mc->mc->deleteAtAll(mc->bufs->active().getBuffer());
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.setAnchorAtAll() - Set selection anchor at all cursors
    // Tum imleclerde secim baglama noktasi ayarla
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setAnchorAtAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) return;
            mc->mc->setAnchorAtAll();
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.clearSelectionAtAll() - Clear selection at all cursors
    // Tum imleclerde secimi temizle
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clearSelectionAtAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) return;
            mc->mc->clearSelectionAtAll();
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.dedup() - Remove duplicate cursors at same position
    // Ayni konumdaki tekrar eden imlecleri kaldir
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "dedup"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) return;
            mc->mc->dedup();
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // multicursor.sort() - Sort cursors by position
    // Imlecleri konuma gore sirala
    jsMC->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "sort"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MCBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->mc) return;
            mc->mc->sort();
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
