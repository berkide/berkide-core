#pragma once
#include <v8.h>

struct EditorContext;

// Register event system operations (on, emit, off, etc.) on the editor.events JS object.
// Editor.events JS nesnesine olay sistemi islemlerini (on, emit, off, vb.) kaydet.
void RegisterEventBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
