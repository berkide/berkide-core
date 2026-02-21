#pragma once
#include <v8.h>

struct EditorContext;

// Register file I/O operations (open, save, readFile, writeFile, etc.) on the editor.file JS object.
// Editor.file JS nesnesine dosya islemlerini (open, save, readFile, writeFile, vb.) kaydet.
void RegisterFileBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
