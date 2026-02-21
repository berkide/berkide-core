#include "MarkBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "MarkManager.h"
#include "buffers.h"
#include "state.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Register editor.marks JS object
// editor.marks JS nesnesini kaydet
void RegisterMarkBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsMarks = v8::Object::New(isolate);

    struct MarkCtx {
        Buffers* bufs;
        MarkManager* marks;
    };
    auto* mctx = new MarkCtx{ctx.buffers, ctx.markManager};

    // marks.set(name, line?, col?) - Set a mark at position (default: cursor)
    // Konumda isaret ayarla (varsayilan: imlec)
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "set"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->bufs || !mc->marks || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string name = v8Str(iso, args[0]);
            auto& st = mc->bufs->active();
            int line = (args.Length() > 1) ? args[1]->Int32Value(ctx).FromMaybe(0) : st.getCursor().getLine();
            int col  = (args.Length() > 2) ? args[2]->Int32Value(ctx).FromMaybe(0) : st.getCursor().getCol();
            mc->marks->set(name, line, col, st.getFilePath());
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.get(name) -> {line, col} | null
    // Isareti al
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "get"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string name = v8Str(iso, args[0]);
            const Mark* m = mc->marks->get(name);
            if (!m) { args.GetReturnValue().SetNull(); return; }

            v8::Local<v8::Object> obj = v8::Object::New(iso);
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "line"),
                v8::Integer::New(iso, m->line)).Check();
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "col"),
                v8::Integer::New(iso, m->col)).Check();
            std::string fp = mc->marks->getFilePath(name);
            if (!fp.empty()) {
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "filePath"),
                    v8::String::NewFromUtf8(iso, fp.c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(obj);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.remove(name) -> bool
    // Isareti kaldir
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "remove"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks || args.Length() < 1) return;
            std::string name = v8Str(args.GetIsolate(), args[0]);
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), mc->marks->remove(name)));
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.list() -> [{name, line, col}]
    // Tum isaretleri listele
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            auto entries = mc->marks->list();
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(entries.size()));
            for (size_t i = 0; i < entries.size(); ++i) {
                v8::Local<v8::Object> obj = v8::Object::New(iso);
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "name"),
                    v8::String::NewFromUtf8(iso, entries[i].first.c_str()).ToLocalChecked()).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "line"),
                    v8::Integer::New(iso, entries[i].second.line)).Check();
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "col"),
                    v8::Integer::New(iso, entries[i].second.col)).Check();
                arr->Set(ctx, static_cast<uint32_t>(i), obj).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.jumpBack() -> {line, col, filePath?} | null
    // Atlama listesinde geri git
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "jumpBack"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks || !mc->bufs) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            // Push current position before jumping
            // Atlamadan once mevcut konumu it
            auto& st = mc->bufs->active();
            mc->marks->pushJump(st.getFilePath(), st.getCursor().getLine(), st.getCursor().getCol());

            JumpEntry entry;
            if (!mc->marks->jumpBack(entry)) {
                args.GetReturnValue().SetNull();
                return;
            }

            st.getCursor().setPosition(entry.line, entry.col);
            v8::Local<v8::Object> obj = v8::Object::New(iso);
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "line"),
                v8::Integer::New(iso, entry.line)).Check();
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "col"),
                v8::Integer::New(iso, entry.col)).Check();
            if (!entry.filePath.empty()) {
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "filePath"),
                    v8::String::NewFromUtf8(iso, entry.filePath.c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(obj);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.jumpForward() -> {line, col, filePath?} | null
    // Atlama listesinde ileri git
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "jumpForward"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks || !mc->bufs) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            JumpEntry entry;
            if (!mc->marks->jumpForward(entry)) {
                args.GetReturnValue().SetNull();
                return;
            }

            mc->bufs->active().getCursor().setPosition(entry.line, entry.col);
            v8::Local<v8::Object> obj = v8::Object::New(iso);
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "line"),
                v8::Integer::New(iso, entry.line)).Check();
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "col"),
                v8::Integer::New(iso, entry.col)).Check();
            if (!entry.filePath.empty()) {
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "filePath"),
                    v8::String::NewFromUtf8(iso, entry.filePath.c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(obj);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.recordEdit(line, col) - Record an edit position for change list
    // Degisiklik listesi icin bir duzenleme konumunu kaydet
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "recordEdit"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks || args.Length() < 2) return;
            auto ctx = args.GetIsolate()->GetCurrentContext();
            int line = args[0]->Int32Value(ctx).FromJust();
            int col  = args[1]->Int32Value(ctx).FromJust();
            mc->marks->recordEdit(line, col);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.prevChange() -> {line, col, filePath?} | null - Navigate to previous change
    // Onceki degisiklige git
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "prevChange"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks || !mc->bufs) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            JumpEntry entry;
            if (!mc->marks->prevChange(entry)) {
                args.GetReturnValue().SetNull();
                return;
            }

            // Move cursor to the change position
            // Imleci degisiklik konumuna tasi
            mc->bufs->active().getCursor().setPosition(entry.line, entry.col);

            v8::Local<v8::Object> obj = v8::Object::New(iso);
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "line"),
                v8::Integer::New(iso, entry.line)).Check();
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "col"),
                v8::Integer::New(iso, entry.col)).Check();
            if (!entry.filePath.empty()) {
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "filePath"),
                    v8::String::NewFromUtf8(iso, entry.filePath.c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(obj);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.nextChange() -> {line, col, filePath?} | null - Navigate to next change
    // Sonraki degisiklige git
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "nextChange"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks || !mc->bufs) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            JumpEntry entry;
            if (!mc->marks->nextChange(entry)) {
                args.GetReturnValue().SetNull();
                return;
            }

            // Move cursor to the change position
            // Imleci degisiklik konumuna tasi
            mc->bufs->active().getCursor().setPosition(entry.line, entry.col);

            v8::Local<v8::Object> obj = v8::Object::New(iso);
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "line"),
                v8::Integer::New(iso, entry.line)).Check();
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "col"),
                v8::Integer::New(iso, entry.col)).Check();
            if (!entry.filePath.empty()) {
                obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "filePath"),
                    v8::String::NewFromUtf8(iso, entry.filePath.c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(obj);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.clearLocal() - Clear buffer-local marks only
    // Yalnizca buffer-yerel isaretleri temizle
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clearLocal"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks) return;
            mc->marks->clearLocal();
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.clearAll() - Clear all marks including global
    // Global dahil tum isaretleri temizle
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clearAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks) return;
            mc->marks->clearAll();
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "marks"),
        jsMarks).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _markReg = [] {
    BindingRegistry::instance().registerBinding("marks", RegisterMarkBinding);
    return true;
}();
