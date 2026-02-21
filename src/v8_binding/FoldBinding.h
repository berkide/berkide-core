#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.folds JS binding (create, remove, toggle, collapse, expand, collapseAll, expandAll, getFoldAt, isLineHidden, list, visibleLineCount, clearAll)
// editor.folds JS binding'ini kaydet (create, remove, toggle, collapse, expand, collapseAll, expandAll, getFoldAt, isLineHidden, list, visibleLineCount, clearAll)
void RegisterFoldBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
