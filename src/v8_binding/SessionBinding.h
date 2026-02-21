#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.session JS binding (save, load, saveAs, loadFrom, list, remove)
// editor.session JS binding'ini kaydet (save, load, saveAs, loadFrom, list, remove)
void RegisterSessionBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
