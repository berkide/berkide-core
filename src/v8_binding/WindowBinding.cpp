#include "WindowBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "WindowManager.h"
#include <v8.h>

// Helper: convert Window to V8 object
// Yardimci: Window'u V8 nesnesine cevir
static v8::Local<v8::Object> windowToV8(v8::Isolate* iso, v8::Local<v8::Context> ctx,
                                          const Window& w) {
    v8::Local<v8::Object> obj = v8::Object::New(iso);
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "id"),
        v8::Integer::New(iso, w.id)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "bufferIndex"),
        v8::Integer::New(iso, w.bufferIndex)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "scrollTop"),
        v8::Integer::New(iso, w.scrollTop)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "cursorLine"),
        v8::Integer::New(iso, w.cursorLine)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "cursorCol"),
        v8::Integer::New(iso, w.cursorCol)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "width"),
        v8::Integer::New(iso, w.width)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "height"),
        v8::Integer::New(iso, w.height)).Check();
    return obj;
}

// Register editor.windows JS object
// editor.windows JS nesnesini kaydet
void RegisterWindowBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsWin = v8::Object::New(isolate);

    auto* mgr = edCtx.windowManager;

    // windows.splitH() -> newWindowId - Split horizontally
    // Yatay bol
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "splitH"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            int id = m->splitActive(SplitDirection::Horizontal);
            args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), id));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.splitV() -> newWindowId - Split vertically
    // Dikey bol
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "splitV"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            int id = m->splitActive(SplitDirection::Vertical);
            args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), id));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.close(windowId?) -> bool - Close a window
    // Pencereyi kapat
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "close"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            auto* iso = args.GetIsolate();
            bool ok;
            if (args.Length() > 0) {
                int id = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
                ok = m->closeWindow(id);
            } else {
                ok = m->closeActive();
            }
            args.GetReturnValue().Set(v8::Boolean::New(iso, ok));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.setActive(windowId) -> bool
    // Aktif pencereyi ayarla
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setActive"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            int id = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            args.GetReturnValue().Set(v8::Boolean::New(iso, m->setActive(id)));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.active() -> window object
    // Aktif pencereyi al
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "active"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            const Window* w = m->active();
            if (w) {
                args.GetReturnValue().Set(windowToV8(iso, ctx, *w));
            } else {
                args.GetReturnValue().SetNull();
            }
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.getWindow(id) -> window | null
    // Kimlige gore pencere al
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getWindow"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            int id = args[0]->Int32Value(ctx).FromJust();
            const Window* w = m->getWindow(id);
            if (w) {
                args.GetReturnValue().Set(windowToV8(iso, ctx, *w));
            } else {
                args.GetReturnValue().SetNull();
            }
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.focusNext() -> bool
    // Sonraki pencereye odaklan
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "focusNext"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), m->focusNext()));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.focusPrev() -> bool
    // Onceki pencereye odaklan
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "focusPrev"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), m->focusPrev()));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.list() -> [windowId, ...]
    // Tum pencere kimliklerini listele
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            auto ids = m->listWindowIds();
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(ids.size()));
            for (size_t i = 0; i < ids.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i), v8::Integer::New(iso, ids[i])).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.count() -> number
    // Pencere sayisini al
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "count"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), m->windowCount()));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.resize(deltaRatio) - Resize active split
    // Aktif bolmeyi yeniden boyutlandir
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "resize"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 1) return;
            double delta = args[0]->NumberValue(args.GetIsolate()->GetCurrentContext()).FromJust();
            m->resizeActive(delta);
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.equalize() - Equalize all splits
    // Tum bolmeleri esitle
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "equalize"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            m->equalize();
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.setLayout(width, height) - Set total layout size
    // Toplam duzen boyutunu ayarla
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setLayout"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m || args.Length() < 2) return;
            auto ctx = args.GetIsolate()->GetCurrentContext();
            int w = args[0]->Int32Value(ctx).FromJust();
            int h = args[1]->Int32Value(ctx).FromJust();
            m->setLayoutSize(w, h);
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.activeId() -> int - Get active window ID
    // Aktif pencere kimligini al
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "activeId"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), m->activeId()));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.focusUp() -> bool - Focus window above
    // Ustteki pencereye odaklan
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "focusUp"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), m->focusUp()));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.focusDown() -> bool - Focus window below
    // Alttaki pencereye odaklan
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "focusDown"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), m->focusDown()));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.focusLeft() -> bool - Focus window to the left
    // Soldaki pencereye odaklan
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "focusLeft"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), m->focusLeft()));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.focusRight() -> bool - Focus window to the right
    // Sagdaki pencereye odaklan
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "focusRight"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), m->focusRight()));
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    // windows.recalcLayout() - Recalculate window dimensions from split tree
    // Bolme agacindan pencere boyutlarini yeniden hesapla
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "recalcLayout"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* m = static_cast<WindowManager*>(args.Data().As<v8::External>()->Value());
            if (!m) return;
            m->recalcLayout();
        }, v8::External::New(isolate, mgr)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "windows"),
        jsWin).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _windowReg = [] {
    BindingRegistry::instance().registerBinding("windows", RegisterWindowBinding);
    return true;
}();
