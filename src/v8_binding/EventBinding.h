// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <v8.h>

struct EditorContext;

// Register event system operations (on, emit, off, etc.) on the editor.events JS object.
// Editor.events JS nesnesine olay sistemi islemlerini (on, emit, off, vb.) kaydet.
void RegisterEventBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
