#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.process JS binding (spawn, write, kill, onStdout, onStderr, onExit)
// editor.process JS binding'ini kaydet (spawn, write, kill, onStdout, onStderr, onExit)
void RegisterProcessBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
