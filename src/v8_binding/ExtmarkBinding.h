#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.extmarks JS binding (set, get, remove, getInRange, getOnLine, clearNamespace, list, count, clearAll)
// editor.extmarks JS binding'ini kaydet (set, get, remove, getInRange, getOnLine, clearNamespace, list, count, clearAll)
void RegisterExtmarkBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
