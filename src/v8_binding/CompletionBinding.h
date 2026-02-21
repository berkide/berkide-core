#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.completion JS binding (filter, score, extractWords)
// editor.completion JS binding'ini kaydet (filter, score, extractWords)
void RegisterCompletionBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
