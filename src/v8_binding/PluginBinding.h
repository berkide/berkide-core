#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.plugins JS binding (list, enable, disable)
// editor.plugins JS binding'ini kaydet (listele, etkinlestir, devre disi birak)
void RegisterPluginBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
