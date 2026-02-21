#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.keymap JS binding (set, remove, lookup, feedKey, resetPrefix, listBindings, listKeymaps, createKeymap)
// editor.keymap JS binding'ini kaydet (set, remove, lookup, feedKey, resetPrefix, listBindings, listKeymaps, createKeymap)
void RegisterKeymapBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
