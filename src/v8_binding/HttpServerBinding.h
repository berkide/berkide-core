#pragma once
#include <v8.h>

struct EditorContext;

// Register HTTP server operations (start, stop, route, etc.) on the editor.http JS object.
// Editor.http JS nesnesine HTTP sunucu islemlerini (start, stop, route, vb.) kaydet.
void RegisterHttpServerBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
