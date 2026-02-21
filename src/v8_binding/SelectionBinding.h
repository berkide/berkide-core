#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.selection JS binding (get, set, clear, getText, getRange, type)
// editor.selection JS binding'ini kaydet (get, set, clear, getText, getRange, type)
void RegisterSelectionBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
