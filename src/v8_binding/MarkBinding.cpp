// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "MarkBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
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

// Helper: convert JumpEntry to json with optional filePath
// Yardimci: JumpEntry'yi istege bagli filePath ile json'a cevir
static json jumpEntryToJson(const JumpEntry& entry) {
    json obj = {
        {"line", entry.line},
        {"col", entry.col}
    };
    if (!entry.filePath.empty()) {
        obj["filePath"] = entry.filePath;
    }
    return obj;
}

// Context struct for mark binding lambdas
// Isaret binding lambda'lari icin baglam yapisi
struct MarkCtx {
    Buffers* bufs;
    MarkManager* marks;
    I18n* i18n;
};

// Register editor.marks JS object
// editor.marks JS nesnesini kaydet
void RegisterMarkBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsMarks = v8::Object::New(isolate);

    auto* mctx = new MarkCtx{ctx.buffers, ctx.markManager, ctx.i18n};

    // marks.set(name, line?, col?) - Set a mark at position (default: cursor)
    // Konumda isaret ayarla (varsayilan: imlec)
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "set"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->bufs || !mc->marks) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "markManager"}}, mc ? mc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name"}}, mc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string name = v8Str(iso, args[0]);
            auto& st = mc->bufs->active();
            int line = (args.Length() > 1) ? args[1]->Int32Value(ctx).FromMaybe(0) : st.getCursor().getLine();
            int col  = (args.Length() > 2) ? args[2]->Int32Value(ctx).FromMaybe(0) : st.getCursor().getCol();
            mc->marks->set(name, line, col, st.getFilePath());
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.get(name) -> {ok, data: {line, col, filePath?} | null, ...}
    // Isareti al
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "get"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "markManager"}}, mc ? mc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name"}}, mc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();

            std::string name = v8Str(iso, args[0]);
            const Mark* m = mc->marks->get(name);
            if (!m) {
                V8Response::ok(args, nullptr);
                return;
            }

            json data = {
                {"line", m->line},
                {"col", m->col}
            };
            std::string fp = mc->marks->getFilePath(name);
            if (!fp.empty()) {
                data["filePath"] = fp;
            }
            V8Response::ok(args, data);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.remove(name) -> {ok, data: bool, ...}
    // Isareti kaldir
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "remove"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "markManager"}}, mc ? mc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name"}}, mc->i18n);
                return;
            }
            std::string name = v8Str(args.GetIsolate(), args[0]);
            bool removed = mc->marks->remove(name);
            V8Response::ok(args, removed);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.list() -> {ok, data: [{name, line, col}], meta: {total: N}, ...}
    // Tum isaretleri listele
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "list"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "markManager"}}, mc ? mc->i18n : nullptr);
                return;
            }

            auto entries = mc->marks->list();
            json arr = json::array();
            for (size_t i = 0; i < entries.size(); ++i) {
                arr.push_back({
                    {"name", entries[i].first},
                    {"line", entries[i].second.line},
                    {"col", entries[i].second.col}
                });
            }
            json meta = {{"total", entries.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.jumpBack() -> {ok, data: {line, col, filePath?} | null, ...}
    // Atlama listesinde geri git
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "jumpBack"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks || !mc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "markManager"}}, mc ? mc->i18n : nullptr);
                return;
            }

            // Push current position before jumping
            // Atlamadan once mevcut konumu it
            auto& st = mc->bufs->active();
            mc->marks->pushJump(st.getFilePath(), st.getCursor().getLine(), st.getCursor().getCol());

            JumpEntry entry;
            if (!mc->marks->jumpBack(entry)) {
                V8Response::ok(args, nullptr);
                return;
            }

            st.getCursor().setPosition(entry.line, entry.col);
            V8Response::ok(args, jumpEntryToJson(entry));
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.jumpForward() -> {ok, data: {line, col, filePath?} | null, ...}
    // Atlama listesinde ileri git
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "jumpForward"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks || !mc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "markManager"}}, mc ? mc->i18n : nullptr);
                return;
            }

            JumpEntry entry;
            if (!mc->marks->jumpForward(entry)) {
                V8Response::ok(args, nullptr);
                return;
            }

            mc->bufs->active().getCursor().setPosition(entry.line, entry.col);
            V8Response::ok(args, jumpEntryToJson(entry));
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.recordEdit(line, col) - Record an edit position for change list
    // Degisiklik listesi icin bir duzenleme konumunu kaydet
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "recordEdit"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "markManager"}}, mc ? mc->i18n : nullptr);
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
            mc->marks->recordEdit(line, col);
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.prevChange() -> {ok, data: {line, col, filePath?} | null, ...} - Navigate to previous change
    // Onceki degisiklige git
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "prevChange"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks || !mc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "markManager"}}, mc ? mc->i18n : nullptr);
                return;
            }

            JumpEntry entry;
            if (!mc->marks->prevChange(entry)) {
                V8Response::ok(args, nullptr);
                return;
            }

            // Move cursor to the change position
            // Imleci degisiklik konumuna tasi
            mc->bufs->active().getCursor().setPosition(entry.line, entry.col);
            V8Response::ok(args, jumpEntryToJson(entry));
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.nextChange() -> {ok, data: {line, col, filePath?} | null, ...} - Navigate to next change
    // Sonraki degisiklige git
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "nextChange"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks || !mc->bufs) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "markManager"}}, mc ? mc->i18n : nullptr);
                return;
            }

            JumpEntry entry;
            if (!mc->marks->nextChange(entry)) {
                V8Response::ok(args, nullptr);
                return;
            }

            // Move cursor to the change position
            // Imleci degisiklik konumuna tasi
            mc->bufs->active().getCursor().setPosition(entry.line, entry.col);
            V8Response::ok(args, jumpEntryToJson(entry));
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.clearLocal() - Clear buffer-local marks only
    // Yalnizca buffer-yerel isaretleri temizle
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clearLocal"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "markManager"}}, mc ? mc->i18n : nullptr);
                return;
            }
            mc->marks->clearLocal();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, mctx)).ToLocalChecked()
    ).Check();

    // marks.clearAll() - Clear all marks including global
    // Global dahil tum isaretleri temizle
    jsMarks->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clearAll"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* mc = static_cast<MarkCtx*>(args.Data().As<v8::External>()->Value());
            if (!mc || !mc->marks) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "markManager"}}, mc ? mc->i18n : nullptr);
                return;
            }
            mc->marks->clearAll();
            V8Response::ok(args, true);
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
