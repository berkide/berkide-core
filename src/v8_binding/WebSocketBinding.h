#pragma once
#include <v8.h>

struct EditorContext;

// Register WebSocket operations (broadcast, onMessage, etc.) on the editor.ws JS object.
// Editor.ws JS nesnesine WebSocket islemlerini (broadcast, onMessage, vb.) kaydet.
void RegisterWebSocketBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
