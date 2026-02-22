// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <v8.h>

struct EditorContext;

// Create and bind the global 'editor' JavaScript object with all registered bindings.
// Tum kayitli binding'lerle global 'editor' JavaScript nesnesini olustur ve bagla.
// Called during V8 initialization and on hot-reload.
// V8 baslatma sirasinda ve anlik yeniden yuklemede cagrilir.
void BindEditor(v8::Isolate* isolate, v8::Local<v8::Context> ctx, EditorContext& edCtx);
