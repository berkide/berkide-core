// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "FoldBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "FoldManager.h"
#include <v8.h>

// Helper: convert Fold to nlohmann::json
// Yardimci: Fold'u nlohmann::json'a cevir
static json foldToJson(const Fold& f) {
    return json({
        {"startLine", f.startLine},
        {"endLine", f.endLine},
        {"collapsed", f.collapsed},
        {"label", f.label}
    });
}

// Context struct for fold binding lambdas
// Katlama binding lambda'lari icin baglam yapisi
struct FoldCtx {
    FoldManager* mgr;
    I18n* i18n;
};

// Register editor.folds JS object with standard response format
// Standart yanit formatiyla editor.folds JS nesnesini kaydet
void RegisterFoldBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsFold = v8::Object::New(isolate);

    auto* fctx = new FoldCtx{edCtx.foldManager, edCtx.i18n};

    // folds.create(startLine, endLine, label?) -> {ok, data: true, ...}
    // Bir katlama bolgesi olustur
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "create"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* fc = static_cast<FoldCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc || !fc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "foldManager"}}, fc ? fc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "startLine, endLine"}}, fc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            int sLine = args[0]->Int32Value(ctx).FromJust();
            int eLine = args[1]->Int32Value(ctx).FromJust();
            std::string label = "";
            if (args.Length() > 2) {
                v8::String::Utf8Value s(iso, args[2]);
                label = *s ? *s : "";
            }
            fc->mgr->create(sLine, eLine, label);
            V8Response::ok(args, true, nullptr, "fold.create.success",
                {{"start", std::to_string(sLine)}, {"end", std::to_string(eLine)}}, fc->i18n);
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // folds.remove(startLine) -> {ok, data: bool, ...}
    // Baslangic satirindaki katlamayi kaldir
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "remove"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* fc = static_cast<FoldCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc || !fc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "foldManager"}}, fc ? fc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "startLine"}}, fc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int sLine = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            bool removed = fc->mgr->remove(sLine);
            if (removed) {
                V8Response::ok(args, true, nullptr, "fold.remove.success",
                    {{"line", std::to_string(sLine)}}, fc->i18n);
            } else {
                V8Response::ok(args, false, nullptr, "fold.remove.not_found",
                    {{"line", std::to_string(sLine)}}, fc->i18n);
            }
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // folds.toggle(line) -> {ok, data: bool, ...}
    // Katlamayi degistir
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "toggle"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* fc = static_cast<FoldCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc || !fc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "foldManager"}}, fc ? fc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "line"}}, fc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int line = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            bool result = fc->mgr->toggle(line);
            V8Response::ok(args, result, nullptr, "fold.toggle.success",
                {{"line", std::to_string(line)}}, fc->i18n);
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // folds.collapse(line) -> {ok, data: bool, ...}
    // Katlamayi kapat
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "collapse"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* fc = static_cast<FoldCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc || !fc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "foldManager"}}, fc ? fc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "line"}}, fc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int line = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            bool result = fc->mgr->collapse(line);
            V8Response::ok(args, result, nullptr, "fold.collapse.success",
                {{"line", std::to_string(line)}}, fc->i18n);
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // folds.expand(line) -> {ok, data: bool, ...}
    // Katlamayi ac
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "expand"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* fc = static_cast<FoldCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc || !fc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "foldManager"}}, fc ? fc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "line"}}, fc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int line = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            bool result = fc->mgr->expand(line);
            V8Response::ok(args, result, nullptr, "fold.expand.success",
                {{"line", std::to_string(line)}}, fc->i18n);
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // folds.collapseAll() -> {ok, data: true, ...}
    // Tum katlamalari kapat
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "collapseAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* fc = static_cast<FoldCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc || !fc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "foldManager"}}, fc ? fc->i18n : nullptr);
                return;
            }
            fc->mgr->collapseAll();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // folds.expandAll() -> {ok, data: true, ...}
    // Tum katlamalari ac
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "expandAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* fc = static_cast<FoldCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc || !fc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "foldManager"}}, fc ? fc->i18n : nullptr);
                return;
            }
            fc->mgr->expandAll();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // folds.getFoldAt(line) -> {ok, data: fold|null, ...}
    // Satirdaki katlamayi al
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getFoldAt"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* fc = static_cast<FoldCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc || !fc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "foldManager"}}, fc ? fc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "line"}}, fc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int line = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            const Fold* f = fc->mgr->getFoldAt(line);
            if (f) {
                V8Response::ok(args, foldToJson(*f));
            } else {
                V8Response::ok(args, nullptr, nullptr, "fold.get.not_found",
                    {{"line", std::to_string(line)}}, fc->i18n);
            }
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // folds.isLineHidden(line) -> {ok, data: bool, ...}
    // Satirin gizli olup olmadigini kontrol et
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isLineHidden"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* fc = static_cast<FoldCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc || !fc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "foldManager"}}, fc ? fc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing", {{"name", "line"}}, fc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int line = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            bool hidden = fc->mgr->isLineHidden(line);
            V8Response::ok(args, hidden);
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // folds.list() -> {ok, data: [fold, ...], meta: {total: N}, ...}
    // Tum katlamalari listele
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* fc = static_cast<FoldCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc || !fc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "foldManager"}}, fc ? fc->i18n : nullptr);
                return;
            }

            auto folds = fc->mgr->list();
            json arr = json::array();
            for (const auto& f : folds) {
                arr.push_back(foldToJson(f));
            }
            json meta = {{"total", folds.size()}};
            V8Response::ok(args, arr, meta, "fold.list.success",
                {{"count", std::to_string(folds.size())}}, fc->i18n);
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // folds.visibleLineCount(totalLines) -> {ok, data: number, ...}
    // Gorunen satir sayisini al
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "visibleLineCount"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* fc = static_cast<FoldCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc || !fc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "foldManager"}}, fc ? fc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "totalLines"}}, fc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            int total = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            int visible = fc->mgr->visibleLineCount(total);
            V8Response::ok(args, visible);
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
    ).Check();

    // folds.clearAll() -> {ok, data: true, ...}
    // Tum katlamalari temizle
    jsFold->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clearAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* fc = static_cast<FoldCtx*>(args.Data().As<v8::External>()->Value());
            if (!fc || !fc->mgr) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "foldManager"}}, fc ? fc->i18n : nullptr);
                return;
            }
            fc->mgr->clearAll();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, fctx)).ToLocalChecked()
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
