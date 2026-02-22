// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "ExtmarkBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "Extmark.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Helper: convert Extmark to json object
// Yardimci: Extmark'i json nesnesine cevir
static json extmarkToJson(const Extmark& em) {
    return json({
        {"id", em.id},
        {"startLine", em.startLine},
        {"startCol", em.startCol},
        {"endLine", em.endLine},
        {"endCol", em.endCol},
        {"ns", em.ns},
        {"type", em.type},
        {"data", em.data}
    });
}

// Context struct for extmark binding lambdas
// Extmark binding lambda'lari icin baglam yapisi
struct ExtmarkCtx {
    ExtmarkManager* mgr;
    I18n* i18n;
};

// Register editor.extmarks JS object
// editor.extmarks JS nesnesini kaydet
void RegisterExtmarkBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsExt = v8::Object::New(isolate);

    auto* ectx = new ExtmarkCtx{edCtx.extmarkManager, edCtx.i18n};

    // extmarks.set(ns, startLine, startCol, endLine, endCol, type?, data?) -> {ok, data: id, ...}
    // Yeni extmark ekle, kimligini dondur
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "set"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<ExtmarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec || !ec->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "extmarkManager"}}, ec ? ec->i18n : nullptr);
                return;
            }
            if (args.Length() < 5) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "ns, startLine, startCol, endLine, endCol"}}, ec->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string ns = v8Str(iso, args[0]);
            int sLine = args[1]->Int32Value(ctx).FromJust();
            int sCol  = args[2]->Int32Value(ctx).FromJust();
            int eLine = args[3]->Int32Value(ctx).FromJust();
            int eCol  = args[4]->Int32Value(ctx).FromJust();

            std::string type = (args.Length() > 5) ? v8Str(iso, args[5]) : "";
            std::string data = (args.Length() > 6) ? v8Str(iso, args[6]) : "";

            int id = ec->mgr->set(ns, sLine, sCol, eLine, eCol, type, data);
            V8Response::ok(args, id);
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
    ).Check();

    // extmarks.get(id) -> {ok, data: extmark | null, ...}
    // Kimlige gore extmark al
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "get"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<ExtmarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec || !ec->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "extmarkManager"}}, ec ? ec->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "id"}}, ec->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int id = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            const Extmark* em = ec->mgr->get(id);
            if (em) {
                V8Response::ok(args, extmarkToJson(*em));
            } else {
                V8Response::ok(args, nullptr);
            }
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
    ).Check();

    // extmarks.remove(id) -> {ok, data: bool, ...}
    // Kimlige gore extmark sil
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "remove"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<ExtmarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec || !ec->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "extmarkManager"}}, ec ? ec->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "id"}}, ec->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int id = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            bool removed = ec->mgr->remove(id);
            V8Response::ok(args, removed);
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
    ).Check();

    // extmarks.clearNamespace(ns) -> {ok, data: removedCount, ...}
    // Ad alanindaki tum extmark'lari sil
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clearNamespace"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<ExtmarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec || !ec->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "extmarkManager"}}, ec ? ec->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "ns"}}, ec->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            std::string ns = v8Str(iso, args[0]);
            int count = ec->mgr->clearNamespace(ns);
            V8Response::ok(args, count);
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
    ).Check();

    // extmarks.getInRange(startLine, endLine, ns?) -> {ok, data: [extmark, ...], meta: {total: N}, ...}
    // Satir araligindaki extmark'lari al
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getInRange"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<ExtmarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec || !ec->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "extmarkManager"}}, ec ? ec->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "startLine, endLine"}}, ec->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            int sLine = args[0]->Int32Value(ctx).FromJust();
            int eLine = args[1]->Int32Value(ctx).FromJust();
            std::string ns = (args.Length() > 2) ? v8Str(iso, args[2]) : "";

            auto marks = ec->mgr->getInRange(sLine, eLine, ns);
            json arr = json::array();
            for (size_t i = 0; i < marks.size(); ++i) {
                arr.push_back(extmarkToJson(*marks[i]));
            }
            json meta = {{"total", marks.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
    ).Check();

    // extmarks.getOnLine(line, ns?) -> {ok, data: [extmark, ...], meta: {total: N}, ...}
    // Belirli bir satirdaki extmark'lari al
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getOnLine"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<ExtmarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec || !ec->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "extmarkManager"}}, ec ? ec->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "line"}}, ec->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            int line = args[0]->Int32Value(ctx).FromJust();
            std::string ns = (args.Length() > 1) ? v8Str(iso, args[1]) : "";

            auto marks = ec->mgr->getOnLine(line, ns);
            json arr = json::array();
            for (size_t i = 0; i < marks.size(); ++i) {
                arr.push_back(extmarkToJson(*marks[i]));
            }
            json meta = {{"total", marks.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
    ).Check();

    // extmarks.list(ns?) -> {ok, data: [extmark, ...], meta: {total: N}, ...}
    // Tum extmark'lari listele
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<ExtmarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec || !ec->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "extmarkManager"}}, ec ? ec->i18n : nullptr);
                return;
            }
            auto* iso = args.GetIsolate();

            std::string ns = (args.Length() > 0) ? v8Str(iso, args[0]) : "";
            auto marks = ec->mgr->list(ns);
            json arr = json::array();
            for (size_t i = 0; i < marks.size(); ++i) {
                arr.push_back(extmarkToJson(*marks[i]));
            }
            json meta = {{"total", marks.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
    ).Check();

    // extmarks.count() -> {ok, data: number, ...}
    // Toplam extmark sayisini al
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "count"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<ExtmarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec || !ec->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "extmarkManager"}}, ec ? ec->i18n : nullptr);
                return;
            }
            int count = static_cast<int>(ec->mgr->count());
            V8Response::ok(args, count);
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
    ).Check();

    // extmarks.clearAll() -> {ok, data: true, ...}
    // Tum extmark'lari temizle
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clearAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<ExtmarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec || !ec->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "extmarkManager"}}, ec ? ec->i18n : nullptr);
                return;
            }
            ec->mgr->clearAll();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
    ).Check();

    // extmarks.setWithVirtText(ns, startLine, startCol, endLine, endCol, virtText, virtTextPos, virtStyle?, type?, data?) -> {ok, data: id, ...}
    // Sanal metinli yeni extmark ekle, kimligini dondur
    jsExt->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setWithVirtText"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<ExtmarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec || !ec->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "extmarkManager"}}, ec ? ec->i18n : nullptr);
                return;
            }
            if (args.Length() < 7) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "ns, startLine, startCol, endLine, endCol, virtText, virtTextPos"}}, ec->i18n);
                return;
            }
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

            int id = ec->mgr->setWithVirtText(ns, sLine, sCol, eLine, eCol, vText, pos, vStyle, type, data);
            V8Response::ok(args, id);
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
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
