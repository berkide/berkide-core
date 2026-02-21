#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.diff JS binding (diff, diffText, unifiedDiff, applyPatch, merge3)
// editor.diff JS binding'ini kaydet (diff, diffText, unifiedDiff, applyPatch, merge3)
void RegisterDiffBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
