#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.multicursor JS binding (add, remove, clear, moveAll, insertAll, list, addNextMatch, addOnLines, count, isActive)
// editor.multicursor JS binding'ini kaydet (add, remove, clear, moveAll, insertAll, list, addNextMatch, addOnLines, count, isActive)
void RegisterMultiCursorBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
