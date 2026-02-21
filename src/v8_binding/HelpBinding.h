#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.help JS binding (show, search, list)
// editor.help JS binding'ini kaydet (goster, ara, listele)
void RegisterHelpBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
