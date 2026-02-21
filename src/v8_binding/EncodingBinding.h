#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.encoding JS binding (detect, detectFile, toUTF8, isValidUTF8, name, parse)
// editor.encoding JS binding'ini kaydet (detect, detectFile, toUTF8, isValidUTF8, name, parse)
void RegisterEncodingBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
