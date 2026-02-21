#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.windows JS binding (split, close, setActive, active, focusNext, focusPrev, list, resize, equalize, getWindow, count)
// editor.windows JS binding'ini kaydet (split, close, setActive, active, focusNext, focusPrev, list, resize, equalize, getWindow, count)
void RegisterWindowBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
