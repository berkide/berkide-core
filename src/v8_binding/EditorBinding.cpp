// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "EditorBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"

// Create the global "editor" JS object and apply all registered bindings (buffer, cursor, etc.)
// Global "editor" JS nesnesini olustur ve tum kayitli binding'leri uygula (buffer, cursor, vb.)
void BindEditor(v8::Isolate* isolate, v8::Local<v8::Context> ctx, EditorContext& edCtx) {
    v8::Local<v8::Object> jsEditor = v8::Object::New(isolate);

    // Provenance tracking: editor.__sources = { cpp: {}, js: {} }
    // Kaynak takibi: her binding'in C++ mi JS mi oldugunu kaydeder
    v8::Local<v8::Object> sources = v8::Object::New(isolate);
    v8::Local<v8::Object> cppSources = v8::Object::New(isolate);
    v8::Local<v8::Object> jsSources = v8::Object::New(isolate);
    sources->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "cpp"), cppSources).Check();
    sources->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "js"), jsSources).Check();
    jsEditor->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "__sources"), sources).Check();

    // Apply all C++ bindings and record their names in __sources.cpp
    // Tum C++ binding'leri uygula ve isimlerini __sources.cpp'ye kaydet
    BindingRegistry::instance().applyAll(isolate, jsEditor, edCtx);
    auto trueVal = v8::Boolean::New(isolate, true);
    for (const auto& name : BindingRegistry::instance().list()) {
        cppSources->Set(ctx,
            v8::String::NewFromUtf8(isolate, name.c_str()).ToLocalChecked(),
            trueVal).Check();
    }

    ctx->Global()->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "editor"), jsEditor).Check();
}
