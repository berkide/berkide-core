// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <v8.h>

struct EditorContext;

// Register cursor operations (moveTo, getX, getY, etc.) on the editor.cursor JS object.
// Editor.cursor JS nesnesine imleç islemlerini (moveTo, getX, getY, vb.) kaydet.
void RegisterCursorBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
