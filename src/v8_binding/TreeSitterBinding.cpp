// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#ifdef BERKIDE_TREESITTER_ENABLED

#include "TreeSitterBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "TreeSitterEngine.h"
#include "buffers.h"
#include "state.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Context for tree-sitter binding
// Tree-sitter binding baglami
struct TSBindCtx {
    TreeSitterEngine* engine;
    Buffers* bufs;
    I18n* i18n;
};

// Helper: convert SyntaxNode to nlohmann::json (recursive)
// Yardimci: SyntaxNode'u nlohmann::json'a cevir (rekursif)
static json nodeToJson(const SyntaxNode& node) {
    json obj = {
        {"type", node.type},
        {"startLine", node.startLine},
        {"startCol", node.startCol},
        {"endLine", node.endLine},
        {"endCol", node.endCol},
        {"isNamed", node.isNamed}
    };
    if (!node.fieldName.empty()) {
        obj["fieldName"] = node.fieldName;
    }

    // Add children array
    // Cocuklar dizisi ekle
    json children = json::array();
    for (const auto& child : node.children) {
        children.push_back(nodeToJson(child));
    }
    obj["children"] = children;

    return obj;
}

// Register editor.treesitter JS object with standard response format
// Standart yanit formatiyla editor.treesitter JS nesnesini kaydet
void RegisterTreeSitterBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsTS = v8::Object::New(isolate);

    auto* tctx = new TSBindCtx{edCtx.treeSitter, edCtx.buffers, edCtx.i18n};

    // treesitter.loadLanguage(name, libraryPath) -> {ok, data: bool}
    // Paylasimli kutuphaneden dil grameri yukle
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "loadLanguage"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "treeSitterEngine"}}, tc ? tc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name, libraryPath"}}, tc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            std::string name = v8Str(iso, args[0]);
            std::string path = v8Str(iso, args[1]);
            bool result = tc->engine->loadLanguage(name, path);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.setLanguage(name) -> {ok, data: bool}
    // Ayristirici icin dili ayarla
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setLanguage"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "treeSitterEngine"}}, tc ? tc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name"}}, tc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            std::string name = v8Str(iso, args[0]);
            bool result = tc->engine->setLanguage(name);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.currentLanguage() -> {ok, data: string}
    // Mevcut dil adini al
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "currentLanguage"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "treeSitterEngine"}}, tc ? tc->i18n : nullptr);
                return;
            }
            const std::string& lang = tc->engine->currentLanguage();
            V8Response::ok(args, lang);
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.hasLanguage(name) -> {ok, data: bool}
    // Dilin yuklu olup olmadigini kontrol et
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "hasLanguage"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "treeSitterEngine"}}, tc ? tc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "name"}}, tc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            std::string name = v8Str(iso, args[0]);
            bool result = tc->engine->hasLanguage(name);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.listLanguages() -> {ok, data: [string, ...], meta: {total: N}}
    // Yuklu dilleri listele
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listLanguages"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "treeSitterEngine"}}, tc ? tc->i18n : nullptr);
                return;
            }
            auto langs = tc->engine->listLanguages();
            json arr = json::array();
            for (const auto& l : langs) {
                arr.push_back(l);
            }
            json meta = {{"total", langs.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.parse(source?) -> {ok, data: bool}
    // Metni ayristir (varsayilan olarak aktif buffer)
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "parse"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "treeSitterEngine"}}, tc ? tc->i18n : nullptr);
                return;
            }
            auto* iso = args.GetIsolate();

            std::string source;
            if (args.Length() > 0) {
                source = v8Str(iso, args[0]);
            } else if (tc->bufs) {
                auto& buf = tc->bufs->active().getBuffer();
                for (int i = 0; i < buf.lineCount(); ++i) {
                    if (i > 0) source += '\n';
                    source += buf.getLine(i);
                }
            }

            bool result = tc->engine->parse(source);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.rootNode() -> {ok, data: node}
    // Kok dugumunu al
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "rootNode"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "treeSitterEngine"}}, tc ? tc->i18n : nullptr);
                return;
            }
            auto node = tc->engine->rootNode();
            V8Response::ok(args, nodeToJson(node));
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.nodeAt(line, col) -> {ok, data: node}
    // Konumdaki dugumu al
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "nodeAt"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "treeSitterEngine"}}, tc ? tc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "line, col"}}, tc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            int line = args[0]->Int32Value(ctx).FromJust();
            int col  = args[1]->Int32Value(ctx).FromJust();
            auto node = tc->engine->nodeAt(line, col);
            V8Response::ok(args, nodeToJson(node));
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.namedNodeAt(line, col) -> {ok, data: node}
    // Konumdaki adlandirilmis dugumu al
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "namedNodeAt"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "treeSitterEngine"}}, tc ? tc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "line, col"}}, tc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            int line = args[0]->Int32Value(ctx).FromJust();
            int col  = args[1]->Int32Value(ctx).FromJust();
            auto node = tc->engine->namedNodeAt(line, col);
            V8Response::ok(args, nodeToJson(node));
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.query(queryStr, source?, startLine?, endLine?) -> {ok, data: [match, ...], meta: {total: N}}
    // Agac uzerinde sorgu calistir
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "query"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "treeSitterEngine"}}, tc ? tc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "queryStr"}}, tc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string queryStr = v8Str(iso, args[0]);

            std::string source;
            if (args.Length() > 1) {
                source = v8Str(iso, args[1]);
            } else if (tc->bufs) {
                auto& buf = tc->bufs->active().getBuffer();
                for (int i = 0; i < buf.lineCount(); ++i) {
                    if (i > 0) source += '\n';
                    source += buf.getLine(i);
                }
            }

            int startLine = (args.Length() > 2) ? args[2]->Int32Value(ctx).FromJust() : 0;
            int endLine   = (args.Length() > 3) ? args[3]->Int32Value(ctx).FromJust() : -1;

            auto matches = tc->engine->query(queryStr, source, startLine, endLine);

            json arr = json::array();
            for (const auto& m : matches) {
                json capArr = json::array();
                for (const auto& cap : m.captures) {
                    capArr.push_back(json({
                        {"name", cap.name},
                        {"text", cap.text},
                        {"startLine", cap.startLine},
                        {"startCol", cap.startCol},
                        {"endLine", cap.endLine},
                        {"endCol", cap.endCol}
                    }));
                }
                arr.push_back(json({
                    {"patternIndex", m.patternIndex},
                    {"captures", capArr}
                }));
            }
            json meta = {{"total", matches.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.errors() -> {ok, data: [node, ...], meta: {total: N}}
    // Soz dizimi hatalarini al
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "errors"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "treeSitterEngine"}}, tc ? tc->i18n : nullptr);
                return;
            }
            auto errs = tc->engine->errors();
            json arr = json::array();
            for (const auto& e : errs) {
                arr.push_back(nodeToJson(e));
            }
            json meta = {{"total", errs.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.hasTree() -> {ok, data: bool}
    // Agacin var olup olmadigini kontrol et
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "hasTree"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "treeSitterEngine"}}, tc ? tc->i18n : nullptr);
                return;
            }
            V8Response::ok(args, tc->engine->hasTree());
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.reset() -> {ok, data: true}
    // Mevcut agaci serbest birak
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "reset"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "treeSitterEngine"}}, tc ? tc->i18n : nullptr);
                return;
            }
            tc->engine->reset();
            V8Response::ok(args, true);
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.editAndReparse(startLine, startCol, oldEndLine, oldEndCol, newEndLine, newEndCol, newSource) -> {ok, data: bool}
    // Duzenleme uygula ve artimsal olarak yeniden ayristir
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "editAndReparse"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "treeSitterEngine"}}, tc ? tc->i18n : nullptr);
                return;
            }
            if (args.Length() < 7) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "startLine, startCol, oldEndLine, oldEndCol, newEndLine, newEndCol, newSource"}}, tc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            int startLine   = args[0]->Int32Value(ctx).FromJust();
            int startCol    = args[1]->Int32Value(ctx).FromJust();
            int oldEndLine  = args[2]->Int32Value(ctx).FromJust();
            int oldEndCol   = args[3]->Int32Value(ctx).FromJust();
            int newEndLine  = args[4]->Int32Value(ctx).FromJust();
            int newEndCol   = args[5]->Int32Value(ctx).FromJust();
            std::string newSource = v8Str(iso, args[6]);

            bool result = tc->engine->editAndReparse(
                startLine, startCol, oldEndLine, oldEndCol,
                newEndLine, newEndCol, newSource);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "treesitter"),
        jsTS).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _tsReg = [] {
    BindingRegistry::instance().registerBinding("treesitter", RegisterTreeSitterBinding);
    return true;
}();

#endif // BERKIDE_TREESITTER_ENABLED
