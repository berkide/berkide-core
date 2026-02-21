#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.autosave JS binding (start, stop, setInterval, listRecovery, etc.)
// editor.autosave JS binding'ini kaydet (baslat, durdur, araligiAyarla, kurtarmaListele, vb.)
void RegisterAutoSaveBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
