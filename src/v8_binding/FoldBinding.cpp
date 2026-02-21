#include "FoldBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "FoldManager.h"
#include <v8.h>

// Helper: convert Fold to V8 object
// Yardimci: Fold'u V8 nesnesine cevir
static v8::Local<v8::Object> foldToV8(v8::Isolate* iso, v8::Local<v8::Context> ctx,
                                        const Fold& f) {
    v8::Local<v8::Object> obj = v8::Object::New(iso);
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "startLine"),
        v8::Integer::New(iso, f.startLine)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "endLine"),
        v8::Integer::New(iso, f.endLine)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "collapsed"),
        v8::Boolean::New(iso, f.collapsed)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "label"),
        v8::String::NewFromUtf8(iso, f.label.c_str()).ToLocalChecked()).Check();
    return obj;
}

// Register editor.folds JS object
// editor.folds JS nesnesini kaydet
void RegisterFoldBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsFold = v8::Object::New(isolate);

    auto* mgr = edCtx.foldManager;

    // folds.create(startLine, endLine, label?) - Create a fold region
    // Bir katlama bolgesi olustur
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "create"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<FoldManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            int sLine = args[0]->Int32Value(ctx).FromJust();
            int eLine = args[1]->Int32Value(ctx).FromJust();
            std::string label = "";
            if (args.Length() > 2) {
                v8::String::Utf8Value s(iso, args[2]);
                label = *s ? *s : "";
            }
            m->create(sLine, eLine, label);
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // folds.remove(startLine) -> bool
    // Baslangic satirindaki katlamayi kaldir
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "remove"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<FoldManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            int sLine = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            args.GetReturnValue().Set(v8::Boolean::New(iso, m->remove(sLine)));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // folds.toggle(line) -> bool
    // Katlamayi degistir
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "toggle"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<FoldManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            int line = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            args.GetReturnValue().Set(v8::Boolean::New(iso, m->toggle(line)));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // folds.collapse(line) -> bool
    // Katlamayi kapat
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "collapse"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<FoldManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            int line = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            args.GetReturnValue().Set(v8::Boolean::New(iso, m->collapse(line)));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // folds.expand(line) -> bool
    // Katlamayi ac
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "expand"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<FoldManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            int line = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            args.GetReturnValue().Set(v8::Boolean::New(iso, m->expand(line)));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // folds.collapseAll() - Collapse all folds
    // Tum katlamalari kapat
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "collapseAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<FoldManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            m->collapseAll();
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // folds.expandAll() - Expand all folds
    // Tum katlamalari ac
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "expandAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<FoldManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            m->expandAll();
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // folds.getFoldAt(line) -> fold | null
    // Satirdaki katlamayi al
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getFoldAt"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<FoldManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            int line = args[0]->Int32Value(ctx).FromJust();
            const Fold* f = m->getFoldAt(line);
            if (f) {
                args.GetReturnValue().Set(foldToV8(iso, ctx, *f));
            } else {
                args.GetReturnValue().SetNull();
            }
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // folds.isLineHidden(line) -> bool
    // Satirin gizli olup olmadigini kontrol et
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isLineHidden"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<FoldManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            int line = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            args.GetReturnValue().Set(v8::Boolean::New(iso, m->isLineHidden(line)));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // folds.list() -> [fold, ...]
    // Tum katlamalari listele
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<FoldManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            auto folds = m->list();
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(folds.size()));
            for (size_t i = 0; i < folds.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i), foldToV8(iso, ctx, folds[i])).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // folds.visibleLineCount(totalLines) -> number
    // Gorunen satir sayisini al
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "visibleLineCount"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<FoldManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            int total = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            args.GetReturnValue().Set(v8::Integer::New(iso, m->visibleLineCount(total)));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // folds.clearAll() - Clear all folds
    // Tum katlamalari temizle
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clearAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<FoldManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            m->clearAll();
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "folds"),
        jsFold).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _foldReg = [] {
    BindingRegistry::instance().registerBinding("folds", RegisterFoldBinding);
    return true;
}();
