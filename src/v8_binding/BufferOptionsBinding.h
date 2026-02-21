#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.options JS binding (setDefault, setLocal, get, list, etc.)
// editor.options JS binding'ini kaydet (varsayilanAyarla, yerelAyarla, al, listele, vb.)
void RegisterBufferOptionsBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
