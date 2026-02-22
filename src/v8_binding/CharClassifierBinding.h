// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.chars JS binding (classify, wordAt, bracketMatch, etc.)
// editor.chars JS binding'ini kaydet (siniflandir, kelimeAl, parantezEsle, vb.)
void RegisterCharClassifierBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
