#include "EditorBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"

// Create the global "editor" JS object and apply all registered bindings (buffer, cursor, etc.)
// Global "editor" JS nesnesini olustur ve tum kayitli binding'leri uygula (buffer, cursor, vb.)
void BindEditor(v8::Isolate* isolate, v8::Local<v8::Context> ctx, EditorContext& edCtx) {
    v8::Local<v8::Object> jsEditor = v8::Object::New(isolate);

    // Set editor.version from CMake project version (single source of truth)
    // CMake proje versiyonundan editor.version ayarla (tek dogru kaynak)
    jsEditor->Set(ctx,
        v8::String::NewFromUtf8Literal(isolate, "version"),
        v8::String::NewFromUtf8(isolate, BERKIDE_VERSION).ToLocalChecked()
    ).Check();

    BindingRegistry::instance().applyAll(isolate, jsEditor, edCtx);
    ctx->Global()->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "editor"), jsEditor).Check();
}
