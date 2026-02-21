#pragma once
#include <v8.h>

struct EditorContext;

// Register multi-buffer management operations (addBuffer, switchBuffer, listBuffers, etc.) on the editor.buffers JS object.
// Editor.buffers JS nesnesine coklu buffer yonetim islemlerini (addBuffer, switchBuffer, listBuffers, vb.) kaydet.
void RegisterBuffersBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
