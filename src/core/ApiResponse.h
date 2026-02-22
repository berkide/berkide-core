// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include "nlohmann/json.hpp"
#include "I18n.h"
#include <string>
#include <unordered_map>

using json = nlohmann::json;

// Standardized API response builder for all BerkIDE endpoints and bindings.
// Tum BerkIDE endpoint'leri ve binding'leri icin standartlastirilmis API yanit olusturucu.
// All responses follow: {ok, data, meta, error, message}
// Tum yanitlar su formati izler: {ok, data, meta, error, message}
namespace ApiResponse {

    // Build a successful response with data and optional meta/message
    // Veri ve istege bagli meta/mesaj ile basarili yanit olustur
    inline json ok(const json& data = nullptr,
                   const json& meta = nullptr,
                   const std::string& message = "") {
        json resp;
        resp["ok"] = true;
        resp["data"] = data;
        resp["meta"] = meta;
        resp["error"] = nullptr;
        resp["message"] = message.empty() ? json(nullptr) : json(message);
        return resp;
    }

    // Build a successful response with i18n message lookup
    // i18n mesaj aramasiyla basarili yanit olustur
    inline json ok(const json& data,
                   const json& meta,
                   const std::string& messageKey,
                   const std::unordered_map<std::string, std::string>& params,
                   const I18n* i18n) {
        std::string msg = i18n ? i18n->t(messageKey, params) : messageKey;
        return ok(data, meta, msg);
    }

    // Build an error response with error code, i18n key, and parameters
    // Hata kodu, i18n anahtari ve parametrelerle hata yaniti olustur
    inline json error(const std::string& code,
                      const std::string& key = "",
                      const std::unordered_map<std::string, std::string>& params = {},
                      const I18n* i18n = nullptr) {
        std::string msg = "";
        if (!key.empty() && i18n) {
            msg = i18n->t(key, params);
        } else if (!key.empty()) {
            msg = key;
        }

        json errObj;
        errObj["code"] = code;
        errObj["key"] = key.empty() ? json(nullptr) : json(key);
        errObj["params"] = json::object();
        for (const auto& [k, v] : params) {
            errObj["params"][k] = v;
        }

        json resp;
        resp["ok"] = false;
        resp["data"] = nullptr;
        resp["meta"] = nullptr;
        resp["error"] = errObj;
        resp["message"] = msg.empty() ? json(nullptr) : json(msg);
        return resp;
    }

} // namespace ApiResponse
