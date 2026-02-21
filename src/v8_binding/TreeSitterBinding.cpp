#ifdef BERKIDE_TREESITTER_ENABLED

#include "TreeSitterBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
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
};

// Helper: convert SyntaxNode to V8 object (shallow - no children for perf)
// Yardimci: SyntaxNode'u V8 nesnesine cevir (sig - performans icin cocuksuz)
static v8::Local<v8::Object> nodeToV8(v8::Isolate* iso, v8::Local<v8::Context> ctx,
                                        const SyntaxNode& node) {
    v8::Local<v8::Object> obj = v8::Object::New(iso);
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "type"),
        v8::String::NewFromUtf8(iso, node.type.c_str()).ToLocalChecked()).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "startLine"),
        v8::Integer::New(iso, node.startLine)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "startCol"),
        v8::Integer::New(iso, node.startCol)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "endLine"),
        v8::Integer::New(iso, node.endLine)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "endCol"),
        v8::Integer::New(iso, node.endCol)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "isNamed"),
        v8::Boolean::New(iso, node.isNamed)).Check();
    if (!node.fieldName.empty()) {
        obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "fieldName"),
            v8::String::NewFromUtf8(iso, node.fieldName.c_str()).ToLocalChecked()).Check();
    }

    // Add children array
    // Cocuklar dizisi ekle
    v8::Local<v8::Array> children = v8::Array::New(iso, static_cast<int>(node.children.size()));
    for (size_t i = 0; i < node.children.size(); ++i) {
        children->Set(ctx, static_cast<uint32_t>(i), nodeToV8(iso, ctx, node.children[i])).Check();
    }
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "children"), children).Check();

    return obj;
}

// Register editor.treesitter JS object
// editor.treesitter JS nesnesini kaydet
void RegisterTreeSitterBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsTS = v8::Object::New(isolate);

    auto* tctx = new TSBindCtx{edCtx.treeSitter, edCtx.buffers};

    // treesitter.loadLanguage(name, libraryPath) -> bool
    // Paylasimli kutuphaneden dil grameri yukle
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "loadLanguage"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            std::string name = v8Str(iso, args[0]);
            std::string path = v8Str(iso, args[1]);
            args.GetReturnValue().Set(v8::Boolean::New(iso, tc->engine->loadLanguage(name, path)));
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.setLanguage(name) -> bool
    // Ayristirici icin dili ayarla
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setLanguage"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            std::string name = v8Str(iso, args[0]);
            args.GetReturnValue().Set(v8::Boolean::New(iso, tc->engine->setLanguage(name)));
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.currentLanguage() -> string
    // Mevcut dil adini al
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "currentLanguage"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) return;
            auto* iso = args.GetIsolate();
            const std::string& lang = tc->engine->currentLanguage();
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(iso, lang.c_str()).ToLocalChecked());
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.hasLanguage(name) -> bool
    // Dilin yuklu olup olmadigini kontrol et
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "hasLanguage"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            std::string name = v8Str(iso, args[0]);
            args.GetReturnValue().Set(v8::Boolean::New(iso, tc->engine->hasLanguage(name)));
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.listLanguages() -> [string, ...]
    // Yuklu dilleri listele
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listLanguages"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            auto langs = tc->engine->listLanguages();
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(langs.size()));
            for (size_t i = 0; i < langs.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i),
                    v8::String::NewFromUtf8(iso, langs[i].c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.parse(source?) -> bool - Parse text (defaults to active buffer)
    // Metni ayristir (varsayilan olarak aktif buffer)
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "parse"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) return;
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

            args.GetReturnValue().Set(v8::Boolean::New(iso, tc->engine->parse(source)));
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.rootNode() -> node
    // Kok dugumunu al
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "rootNode"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            auto node = tc->engine->rootNode();
            args.GetReturnValue().Set(nodeToV8(iso, ctx, node));
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.nodeAt(line, col) -> node
    // Konumdaki dugumu al
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "nodeAt"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            int line = args[0]->Int32Value(ctx).FromJust();
            int col  = args[1]->Int32Value(ctx).FromJust();
            auto node = tc->engine->nodeAt(line, col);
            args.GetReturnValue().Set(nodeToV8(iso, ctx, node));
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.namedNodeAt(line, col) -> node
    // Konumdaki adlandirilmis dugumu al
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "namedNodeAt"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            int line = args[0]->Int32Value(ctx).FromJust();
            int col  = args[1]->Int32Value(ctx).FromJust();
            auto node = tc->engine->namedNodeAt(line, col);
            args.GetReturnValue().Set(nodeToV8(iso, ctx, node));
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.query(queryStr, source?, startLine?, endLine?) -> [match, ...]
    // Agac uzerinde sorgu calistir
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "query"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine || args.Length() < 1) return;
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

            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(matches.size()));
            for (size_t i = 0; i < matches.size(); ++i) {
                v8::Local<v8::Object> mObj = v8::Object::New(iso);
                mObj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "patternIndex"),
                    v8::Integer::New(iso, matches[i].patternIndex)).Check();

                v8::Local<v8::Array> capArr = v8::Array::New(iso,
                    static_cast<int>(matches[i].captures.size()));
                for (size_t j = 0; j < matches[i].captures.size(); ++j) {
                    auto& cap = matches[i].captures[j];
                    v8::Local<v8::Object> cObj = v8::Object::New(iso);
                    cObj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "name"),
                        v8::String::NewFromUtf8(iso, cap.name.c_str()).ToLocalChecked()).Check();
                    cObj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "text"),
                        v8::String::NewFromUtf8(iso, cap.text.c_str()).ToLocalChecked()).Check();
                    cObj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "startLine"),
                        v8::Integer::New(iso, cap.startLine)).Check();
                    cObj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "startCol"),
                        v8::Integer::New(iso, cap.startCol)).Check();
                    cObj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "endLine"),
                        v8::Integer::New(iso, cap.endLine)).Check();
                    cObj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "endCol"),
                        v8::Integer::New(iso, cap.endCol)).Check();
                    capArr->Set(ctx, static_cast<uint32_t>(j), cObj).Check();
                }
                mObj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "captures"), capArr).Check();
                arr->Set(ctx, static_cast<uint32_t>(i), mObj).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.errors() -> [node, ...]
    // Soz dizimi hatalarini al
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "errors"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            auto errs = tc->engine->errors();
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(errs.size()));
            for (size_t i = 0; i < errs.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i), nodeToV8(iso, ctx, errs[i])).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.hasTree() -> bool
    // Agacin var olup olmadigini kontrol et
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "hasTree"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) { args.GetReturnValue().Set(false); return; }
            args.GetReturnValue().Set(v8::Boolean::New(args.GetIsolate(), tc->engine->hasTree()));
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.reset() - Free current tree
    // Mevcut agaci serbest birak
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "reset"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine) return;
            tc->engine->reset();
        }, v8::External::New(isolate, tctx)).ToLocalChecked()
    ).Check();

    // treesitter.editAndReparse(startLine, startCol, oldEndLine, oldEndCol, newEndLine, newEndCol, newSource) -> bool
    // Duzenleme uygula ve artimsal olarak yeniden ayristir
    jsTS->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "editAndReparse"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* tc = static_cast<TSBindCtx*>(args.Data().As<v8::External>()->Value());
            if (!tc || !tc->engine || args.Length() < 7) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            int startLine   = args[0]->Int32Value(ctx).FromJust();
            int startCol    = args[1]->Int32Value(ctx).FromJust();
            int oldEndLine  = args[2]->Int32Value(ctx).FromJust();
            int oldEndCol   = args[3]->Int32Value(ctx).FromJust();
            int newEndLine  = args[4]->Int32Value(ctx).FromJust();
            int newEndCol   = args[5]->Int32Value(ctx).FromJust();
            std::string newSource = v8Str(iso, args[6]);

            bool ok = tc->engine->editAndReparse(
                startLine, startCol, oldEndLine, oldEndCol,
                newEndLine, newEndCol, newSource);
            args.GetReturnValue().Set(v8::Boolean::New(iso, ok));
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
