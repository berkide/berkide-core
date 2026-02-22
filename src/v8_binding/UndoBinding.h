// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <v8.h>

struct EditorContext;

// Register undo/redo operations (undo, redo, checkpoint, etc.) on the editor.undo JS object.
// Editor.undo JS nesnesine geri al/yinele islemlerini (undo, redo, checkpoint, vb.) kaydet.
void RegisterUndoBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
