#include "ExtmarkBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "Extmark.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Helper: convert Extmark to V8 object
// Yardimci: Extmark'i V8 nesnesine cevir
static v8::Local<v8::Object> extmarkToV8(v8::Isolate* iso, v8::Local<v8::Context> ctx,
                                           const Extmark& em) {
    v8::Local<v8::Object> obj = v8::Object::New(iso);
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "id"),
        v8::Integer::New(iso, em.id)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "startLine"),
        v8::Integer::New(iso, em.startLine)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "startCol"),
        v8::Integer::New(iso, em.startCol)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "endLine"),
        v8::Integer::New(iso, em.endLine)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "endCol"),
        v8::Integer::New(iso, em.endCol)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "ns"),
        v8::String::NewFromUtf8(iso, em.ns.c_str()).ToLocalChecked()).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "type"),
        v8::String::NewFromUtf8(iso, em.type.c_str()).ToLocalChecked()).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "data"),
        v8::String::NewFromUtf8(iso, em.data.c_str()).ToLocalChecked()).Check();
    return obj;
}

// Register editor.extmarks JS object
// editor.extmarks JS nesnesini kaydet
void RegisterExtmarkBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsExt = v8::Object::New(isolate);

    auto* mgr = edCtx.extmarkManager;

    // extmarks.set(ns, startLine, startCol, endLine, endCol, type?, data?) -> id
    // Yeni extmark ekle, kimligini dondur
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "set"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<ExtmarkManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 5) return;
            auto* iso = args.GetIsolate();

            std::string ns = v8Str(iso, args[0]);
            int sLine = args[1]->Int32Value(iso->GetCurrentContext()).FromJust();
            int sCol  = args[2]->Int32Value(iso->GetCurrentContext()).FromJust();
            int eLine = args[3]->Int32Value(iso->GetCurrentContext()).FromJust();
            int eCol  = args[4]->Int32Value(iso->GetCurrentContext()).FromJust();

            std::string type = (args.Length() > 5) ? v8Str(iso, args[5]) : "";
            std::string data = (args.Length() > 6) ? v8Str(iso, args[6]) : "";

            int id = m->set(ns, sLine, sCol, eLine, eCol, type, data);
            args.GetReturnValue().Set(v8::Integer::New(iso, id));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // extmarks.get(id) -> extmark | null
    // Kimlige gore extmark al
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "get"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<ExtmarkManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            int id = args[0]->Int32Value(ctx).FromJust();
            const Extmark* em = m->get(id);
            if (em) {
                args.GetReturnValue().Set(extmarkToV8(iso, ctx, *em));
            } else {
                args.GetReturnValue().SetNull();
            }
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // extmarks.remove(id) -> bool
    // Kimlige gore extmark sil
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "remove"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<ExtmarkManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            int id = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            args.GetReturnValue().Set(v8::Boolean::New(iso, m->remove(id)));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // extmarks.clearNamespace(ns) -> number removed
    // Ad alanindaki tum extmark'lari sil
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clearNamespace"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<ExtmarkManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            std::string ns = v8Str(iso, args[0]);
            int count = m->clearNamespace(ns);
            args.GetReturnValue().Set(v8::Integer::New(iso, count));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // extmarks.getInRange(startLine, endLine, ns?) -> [extmark, ...]
    // Satir araligindaki extmark'lari al
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getInRange"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<ExtmarkManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            int sLine = args[0]->Int32Value(ctx).FromJust();
            int eLine = args[1]->Int32Value(ctx).FromJust();
            std::string ns = (args.Length() > 2) ? v8Str(iso, args[2]) : "";

            auto marks = m->getInRange(sLine, eLine, ns);
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(marks.size()));
            for (size_t i = 0; i < marks.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i), extmarkToV8(iso, ctx, *marks[i])).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // extmarks.getOnLine(line, ns?) -> [extmark, ...]
    // Belirli bir satirdaki extmark'lari al
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getOnLine"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<ExtmarkManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            int line = args[0]->Int32Value(ctx).FromJust();
            std::string ns = (args.Length() > 1) ? v8Str(iso, args[1]) : "";

            auto marks = m->getOnLine(line, ns);
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(marks.size()));
            for (size_t i = 0; i < marks.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i), extmarkToV8(iso, ctx, *marks[i])).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // extmarks.list(ns?) -> [extmark, ...]
    // Tum extmark'lari listele
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<ExtmarkManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string ns = (args.Length() > 0) ? v8Str(iso, args[0]) : "";
            auto marks = m->list(ns);
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(marks.size()));
            for (size_t i = 0; i < marks.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i), extmarkToV8(iso, ctx, *marks[i])).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // extmarks.count() -> number
    // Toplam extmark sayisini al
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "count"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<ExtmarkManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(),
                static_cast<int>(m->count())));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // extmarks.clearAll()
    // Tum extmark'lari temizle
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clearAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<ExtmarkManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            m->clearAll();
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // extmarks.setWithVirtText(ns, startLine, startCol, endLine, endCol, virtText, virtTextPos, virtStyle?, type?, data?) -> id
    // Sanal metinli yeni extmark ekle, kimligini dondur
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setWithVirtText"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<ExtmarkManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 7) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string ns    = v8Str(iso, args[0]);
            int sLine         = args[1]->Int32Value(ctx).FromJust();
            int sCol          = args[2]->Int32Value(ctx).FromJust();
            int eLine         = args[3]->Int32Value(ctx).FromJust();
            int eCol          = args[4]->Int32Value(ctx).FromJust();
            std::string vText = v8Str(iso, args[5]);
            std::string vPos  = v8Str(iso, args[6]);

            // Convert virtTextPos string to VirtTextPos enum
            // virtTextPos metnini VirtTextPos enum'una cevir
            VirtTextPos pos = VirtTextPos::None;
            if (vPos == "eol")        pos = VirtTextPos::EOL;
            else if (vPos == "inline")     pos = VirtTextPos::Inline;
            else if (vPos == "overlay")    pos = VirtTextPos::Overlay;
            else if (vPos == "rightAlign") pos = VirtTextPos::RightAlign;

            std::string vStyle = (args.Length() > 7) ? v8Str(iso, args[7]) : "";
            std::string type   = (args.Length() > 8) ? v8Str(iso, args[8]) : "";
            std::string data   = (args.Length() > 9) ? v8Str(iso, args[9]) : "";

            int id = m->setWithVirtText(ns, sLine, sCol, eLine, eCol, vText, pos, vStyle, type, data);
            args.GetReturnValue().Set(v8::Integer::New(iso, id));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "extmarks"),
        jsExt).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _extmarkReg = [] {
    BindingRegistry::instance().registerBinding("extmarks", RegisterExtmarkBinding);
    return true;
}();
