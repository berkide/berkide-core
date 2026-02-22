// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once

#ifdef BERKIDE_TREESITTER_ENABLED

#include <v8.h>

struct EditorContext;

// Register editor.treesitter JS binding (loadLanguage, setLanguage, parse, editAndReparse, nodeAt, query, errors, rootNode, reset, listLanguages)
// editor.treesitter JS binding'ini kaydet (loadLanguage, setLanguage, parse, editAndReparse, nodeAt, query, errors, rootNode, reset, listLanguages)
void RegisterTreeSitterBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);

#endif // BERKIDE_TREESITTER_ENABLED
