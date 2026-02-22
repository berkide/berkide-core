// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "WindowBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "WindowManager.h"
#include <v8.h>

// Helper: convert Window to json object
// Yardimci: Window'u json nesnesine cevir
static json windowToJson(const Window& w) {
    return json({
        {"id", w.id},
        {"bufferIndex", w.bufferIndex},
        {"scrollTop", w.scrollTop},
        {"cursorLine", w.cursorLine},
        {"cursorCol", w.cursorCol},
        {"width", w.width},
        {"height", w.height}
    });
}

// Context struct for window binding lambdas
// Pencere binding lambda'lari icin baglam yapisi
struct WindowCtx {
    WindowManager* mgr;
    I18n* i18n;
};

// Register editor.windows JS object
// editor.windows JS nesnesini kaydet
void RegisterWindowBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsWin = v8::Object::New(isolate);

    auto* wctx = new WindowCtx{edCtx.windowManager, edCtx.i18n};

    // windows.splitH() -> {ok, data: newWindowId, ...} - Split horizontally
    // Yatay bol
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "splitH"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            int id = wc->mgr->splitActive(SplitDirection::Horizontal);
            V8Response::ok(args, id);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.splitV() -> {ok, data: newWindowId, ...} - Split vertically
    // Dikey bol
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "splitV"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            int id = wc->mgr->splitActive(SplitDirection::Vertical);
            V8Response::ok(args, id);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.close(windowId?) -> {ok, data: bool, ...} - Close a window
    // Pencereyi kapat
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "close"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            auto* iso = args.GetIsolate();
            bool result;
            if (args.Length() > 0) {
                int id = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
                result = wc->mgr->closeWindow(id);
            } else {
                result = wc->mgr->closeActive();
            }
            V8Response::ok(args, result);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.setActive(windowId) -> {ok, data: bool, ...}
    // Aktif pencereyi ayarla
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setActive"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "windowId"}}, wc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int id = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            bool result = wc->mgr->setActive(id);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.active() -> {ok, data: window | null, ...}
    // Aktif pencereyi al
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "active"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            const Window* w = wc->mgr->active();
            if (w) {
                V8Response::ok(args, windowToJson(*w));
            } else {
                V8Response::ok(args, nullptr);
            }
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.getWindow(id) -> {ok, data: window | null, ...}
    // Kimlige gore pencere al
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getWindow"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "id"}}, wc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int id = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            const Window* w = wc->mgr->getWindow(id);
            if (w) {
                V8Response::ok(args, windowToJson(*w));
            } else {
                V8Response::ok(args, nullptr);
            }
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.focusNext() -> {ok, data: bool, ...}
    // Sonraki pencereye odaklan
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "focusNext"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            bool result = wc->mgr->focusNext();
            V8Response::ok(args, result);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.focusPrev() -> {ok, data: bool, ...}
    // Onceki pencereye odaklan
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "focusPrev"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            bool result = wc->mgr->focusPrev();
            V8Response::ok(args, result);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.list() -> {ok, data: [windowId, ...], meta: {total: N}, ...}
    // Tum pencere kimliklerini listele
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            auto ids = wc->mgr->listWindowIds();
            json arr = json::array();
            for (size_t i = 0; i < ids.size(); ++i) {
                arr.push_back(ids[i]);
            }
            json meta = {{"total", ids.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.count() -> {ok, data: number, ...}
    // Pencere sayisini al
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "count"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            int count = wc->mgr->windowCount();
            V8Response::ok(args, count);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.resize(deltaRatio) - Resize active split
    // Aktif bolmeyi yeniden boyutlandir
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "resize"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "deltaRatio"}}, wc->i18n);
                return;
            }
            double delta = args[0]->NumberValue(args.GetIsolate()->GetCurrentContext()).FromJust();
            wc->mgr->resizeActive(delta);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.equalize() - Equalize all splits
    // Tum bolmeleri esitle
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "equalize"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            wc->mgr->equalize();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.setLayout(width, height) - Set total layout size
    // Toplam duzen boyutunu ayarla
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setLayout"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "width, height"}}, wc->i18n);
                return;
            }
            auto ctx = args.GetIsolate()->GetCurrentContext();
            int w = args[0]->Int32Value(ctx).FromJust();
            int h = args[1]->Int32Value(ctx).FromJust();
            wc->mgr->setLayoutSize(w, h);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.activeId() -> {ok, data: int, ...} - Get active window ID
    // Aktif pencere kimligini al
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "activeId"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            int id = wc->mgr->activeId();
            V8Response::ok(args, id);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.focusUp() -> {ok, data: bool, ...} - Focus window above
    // Ustteki pencereye odaklan
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "focusUp"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            bool result = wc->mgr->focusUp();
            V8Response::ok(args, result);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.focusDown() -> {ok, data: bool, ...} - Focus window below
    // Alttaki pencereye odaklan
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "focusDown"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            bool result = wc->mgr->focusDown();
            V8Response::ok(args, result);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.focusLeft() -> {ok, data: bool, ...} - Focus window to the left
    // Soldaki pencereye odaklan
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "focusLeft"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            bool result = wc->mgr->focusLeft();
            V8Response::ok(args, result);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.focusRight() -> {ok, data: bool, ...} - Focus window to the right
    // Sagdaki pencereye odaklan
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "focusRight"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            bool result = wc->mgr->focusRight();
            V8Response::ok(args, result);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
    ).Check();

    // windows.recalcLayout() - Recalculate window dimensions from split tree
    // Bolme agacindan pencere boyutlarini yeniden hesapla
    jsWin->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "recalcLayout"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* wc = static_cast<WindowCtx*>(args.Data().As<v8::External>()->Value());
            if (!wc || !wc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "windowManager"}}, wc ? wc->i18n : nullptr);
                return;
            }
            wc->mgr->recalcLayout();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, wctx)).ToLocalChecked()
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
