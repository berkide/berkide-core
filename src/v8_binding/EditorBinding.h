#pragma once
#include <v8.h>

struct EditorContext;

// Create and bind the global 'editor' JavaScript object with all registered bindings.
// Tum kayitli binding'lerle global 'editor' JavaScript nesnesini olustur ve bagla.
// Called during V8 initialization and on hot-reload.
// V8 baslatma sirasinda ve anlik yeniden yuklemede cagrilir.
void BindEditor(v8::Isolate* isolate, v8::Local<v8::Context> ctx, EditorContext& edCtx);
