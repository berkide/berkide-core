#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.macro JS binding (record, stop, play, isRecording, list, clear)
// editor.macro JS binding'ini kaydet (record, stop, play, isRecording, list, clear)
void RegisterMacroBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
