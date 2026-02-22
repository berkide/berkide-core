// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "V8ResponseBuilder.h"
#include "ApiResponse.h"

namespace V8Response {

// Recursively convert nlohmann::json to v8::Value
// nlohmann::json'u rekursif olarak v8::Value'ya donustur
v8::Local<v8::Value> jsonToV8(v8::Isolate* isolate, v8::Local<v8::Context> ctx, const json& j) {
    if (j.is_null()) {
        return v8::Null(isolate);
    }
    if (j.is_boolean()) {
        return v8::Boolean::New(isolate, j.get<bool>());
    }
    if (j.is_number_integer()) {
        int64_t val = j.get<int64_t>();
        if (val >= INT32_MIN && val <= INT32_MAX) {
            return v8::Integer::New(isolate, static_cast<int32_t>(val));
        }
        return v8::Number::New(isolate, static_cast<double>(val));
    }
    if (j.is_number_float()) {
        return v8::Number::New(isolate, j.get<double>());
    }
    if (j.is_string()) {
        return v8::String::NewFromUtf8(isolate, j.get<std::string>().c_str()).ToLocalChecked();
    }
    if (j.is_array()) {
        v8::Local<v8::Array> arr = v8::Array::New(isolate, static_cast<int>(j.size()));
        for (size_t i = 0; i < j.size(); ++i) {
            arr->Set(ctx, static_cast<uint32_t>(i), jsonToV8(isolate, ctx, j[i])).Check();
        }
        return arr;
    }
    if (j.is_object()) {
        v8::Local<v8::Object> obj = v8::Object::New(isolate);
        for (auto& [key, val] : j.items()) {
            obj->Set(ctx,
                v8::String::NewFromUtf8(isolate, key.c_str()).ToLocalChecked(),
                jsonToV8(isolate, ctx, val)).Check();
        }
        return obj;
    }

    // Fallback: undefined for unknown types
    // Geri donus: bilinmeyen tipler icin undefined
    return v8::Undefined(isolate);
}

// Build and set a successful V8 response object from JSON data
// JSON verisinden basarili V8 yanit nesnesi olustur ve ayarla
void ok(const v8::FunctionCallbackInfo<v8::Value>& args,
        const json& data,
        const json& meta,
        const std::string& message) {
    json resp = ApiResponse::ok(data, meta, message);
    auto* isolate = args.GetIsolate();
    auto ctx = isolate->GetCurrentContext();
    args.GetReturnValue().Set(jsonToV8(isolate, ctx, resp));
}

// Build and set a successful V8 response with i18n message lookup
// i18n mesaj aramasiyla basarili V8 yaniti olustur ve ayarla
void ok(const v8::FunctionCallbackInfo<v8::Value>& args,
        const json& data,
        const json& meta,
        const std::string& messageKey,
        const std::unordered_map<std::string, std::string>& params,
        const I18n* i18n) {
    json resp = ApiResponse::ok(data, meta, messageKey, params, i18n);
    auto* isolate = args.GetIsolate();
    auto ctx = isolate->GetCurrentContext();
    args.GetReturnValue().Set(jsonToV8(isolate, ctx, resp));
}

// Build and set an error V8 response object
// Hata V8 yanit nesnesi olustur ve ayarla
void error(const v8::FunctionCallbackInfo<v8::Value>& args,
           const std::string& code,
           const std::string& key,
           const std::unordered_map<std::string, std::string>& params,
           const I18n* i18n) {
    json resp = ApiResponse::error(code, key, params, i18n);
    auto* isolate = args.GetIsolate();
    auto ctx = isolate->GetCurrentContext();
    args.GetReturnValue().Set(jsonToV8(isolate, ctx, resp));
}

} // namespace V8Response
