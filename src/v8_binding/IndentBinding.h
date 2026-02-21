#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.indent JS binding (config, forNewLine, forLine, increase, decrease, reindent)
// editor.indent JS binding'ini kaydet (yapilandirma, yeniSatir, satirIcin, artir, azalt, yenidenGirintile)
void RegisterIndentBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
