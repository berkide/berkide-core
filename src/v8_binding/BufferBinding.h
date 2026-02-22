// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <v8.h>

struct EditorContext;

// Register buffer operations (insertChar, deleteChar, getLine, etc.) on the editor.buffer JS object.
// Editor.buffer JS nesnesine buffer islemlerini (insertChar, deleteChar, getLine, vb.) kaydet.
void RegisterBufferBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
