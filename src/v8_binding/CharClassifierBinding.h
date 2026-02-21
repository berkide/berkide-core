#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.chars JS binding (classify, wordAt, bracketMatch, etc.)
// editor.chars JS binding'ini kaydet (siniflandir, kelimeAl, parantezEsle, vb.)
void RegisterCharClassifierBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
