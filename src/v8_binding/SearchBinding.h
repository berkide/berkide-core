#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.search JS binding (find, findNext, findPrev, findAll, replace, replaceAll, count)
// editor.search JS binding'ini kaydet (find, findNext, findPrev, findAll, replace, replaceAll, count)
void RegisterSearchBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
