#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.wasm JS binding (loadFile, isSupported)
// editor.wasm JS binding'ini kaydet (dosyaYukle, destekleniyor)
void RegisterWasmBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
