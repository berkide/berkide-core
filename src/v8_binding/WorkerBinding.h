#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.workers JS binding (create, postMessage, terminate, list)
// editor.workers JS binding'ini kaydet (olustur, mesajGonder, sonlandir, listele)
void RegisterWorkerBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
