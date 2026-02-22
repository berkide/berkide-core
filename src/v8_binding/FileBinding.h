// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <v8.h>
#include <string>

struct EditorContext;

// Get calling JS file's absolute path from V8 stack trace
// V8 stack trace'den cagiran JS dosyasinin mutlak yolunu al
std::string getCallerScriptPath(v8::Isolate* isolate);

// Register file I/O operations (open, save, readFile, writeFile, etc.) on the editor.file JS object.
// Editor.file JS nesnesine dosya islemlerini (open, save, readFile, writeFile, vb.) kaydet.
void RegisterFileBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
