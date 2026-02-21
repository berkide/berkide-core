#pragma once

#ifdef BERKIDE_TREESITTER_ENABLED

#include <v8.h>

struct EditorContext;

// Register editor.treesitter JS binding (loadLanguage, setLanguage, parse, editAndReparse, nodeAt, query, errors, rootNode, reset, listLanguages)
// editor.treesitter JS binding'ini kaydet (loadLanguage, setLanguage, parse, editAndReparse, nodeAt, query, errors, rootNode, reset, listLanguages)
void RegisterTreeSitterBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);

#endif // BERKIDE_TREESITTER_ENABLED
