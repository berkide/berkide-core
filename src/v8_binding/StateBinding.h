#pragma once
#include <v8.h>

struct EditorContext;

// Register editor state operations (getMode, setMode, etc.) on the editor.state JS object.
// Editor.state JS nesnesine durum islemlerini (getMode, setMode, vb.) kaydet.
void RegisterStateBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
