// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <v8.h>
#include "nlohmann/json.hpp"
#include "I18n.h"
#include <string>
#include <unordered_map>

using json = nlohmann::json;

// V8 response helpers that convert standardized ApiResponse JSON to V8 objects.
// Standartlastirilmis ApiResponse JSON'unu V8 nesnelerine donusturen V8 yanit yardimcilari.
// Used by all V8 bindings to return consistent {ok, data, meta, error, message} format.
// Tutarli {ok, data, meta, error, message} formati dondurmek icin tum V8 binding'leri tarafindan kullanilir.
namespace V8Response {

    // Convert a nlohmann::json value to a v8::Local<v8::Value> recursively
    // Bir nlohmann::json degerini rekursif olarak v8::Local<v8::Value>'ya donustur
    v8::Local<v8::Value> jsonToV8(v8::Isolate* isolate, v8::Local<v8::Context> ctx, const json& j);

    // Set a successful response as return value: {ok: true, data: ..., meta: ..., error: null, message: ...}
    // Basarili yaniti donus degeri olarak ayarla
    void ok(const v8::FunctionCallbackInfo<v8::Value>& args,
            const json& data = nullptr,
            const json& meta = nullptr,
            const std::string& message = "");

    // Set a successful response with i18n message
    // i18n mesajiyla basarili yanit ayarla
    void ok(const v8::FunctionCallbackInfo<v8::Value>& args,
            const json& data,
            const json& meta,
            const std::string& messageKey,
            const std::unordered_map<std::string, std::string>& params,
            const I18n* i18n);

    // Set an error response as return value: {ok: false, data: null, meta: null, error: {...}, message: ...}
    // Hata yanitini donus degeri olarak ayarla
    void error(const v8::FunctionCallbackInfo<v8::Value>& args,
               const std::string& code,
               const std::string& key = "",
               const std::unordered_map<std::string, std::string>& params = {},
               const I18n* i18n = nullptr);

} // namespace V8Response
