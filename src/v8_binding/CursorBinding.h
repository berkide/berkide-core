#pragma once
#include <v8.h>

struct EditorContext;

// Register cursor operations (moveTo, getX, getY, etc.) on the editor.cursor JS object.
// Editor.cursor JS nesnesine imleç islemlerini (moveTo, getX, getY, vb.) kaydet.
void RegisterCursorBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
