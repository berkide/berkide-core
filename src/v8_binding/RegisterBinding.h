#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.registers JS binding (get, set, yank, paste, list)
// editor.registers JS binding'ini kaydet (get, set, yank, paste, list)
void RegisterRegisterBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
