// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <v8.h>

struct EditorContext;

// Register i18n operations (t, setLocale, locale, register, has, locales, keys) on the editor.i18n JS object.
// Editor.i18n JS nesnesine i18n islemlerini (t, setLocale, locale, register, has, locales, keys) kaydet.
void RegisterI18nBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx);
