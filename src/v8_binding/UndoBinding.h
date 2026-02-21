#pragma once
#include <v8.h>

struct EditorContext;

// Register undo/redo operations (undo, redo, checkpoint, etc.) on the editor.undo JS object.
// Editor.undo JS nesnesine geri al/yinele islemlerini (undo, redo, checkpoint, vb.) kaydet.
void RegisterUndoBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
