#pragma once
#include <v8.h>

struct EditorContext;

// Register input handling operations (onKey, feedKey, etc.) on the editor JS object.
// Editor JS nesnesine girdi isleme fonksiyonlarini (onKey, feedKey, vb.) kaydet.
void RegisterInputBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
