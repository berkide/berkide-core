// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include <vector>
#include <functional>
#include "http/httplib.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// Metadata for a single API endpoint.
// Tek bir API endpoint'i icin metadata.
struct EndpointInfo {
    std::string method;       // "GET" or "POST" / HTTP metodu
    std::string path;         // Route path e.g. "/api/state" / Rota yolu
    std::string description;  // Human-readable description / Okunabilir aciklama
    bool authRequired;        // Whether auth is needed / Kimlik dogrulama gerekli mi
    json params;              // Path/query parameter schema / Yol/sorgu parametre semasi
    json body;                // Request body schema (POST only) / Istek govde semasi (sadece POST)
};

// Central registry for all HTTP API endpoints.
// Tum HTTP API endpoint'leri icin merkezi kayit defteri.
// Each endpoint is defined ONCE — the registry stores both the metadata and the httplib route.
// Her endpoint TEK SEFER tanimlanir — kayit defteri hem metadata'yi hem httplib rotasini saklar.
class EndpointRegistry {
public:
    // Auth checker function type
    // Kimlik dogrulama kontrol fonksiyon tipi
    using AuthChecker = std::function<bool(const httplib::Request&, httplib::Response&)>;

    // Set the auth checker function (called before handlers that require auth)
    // Kimlik dogrulama kontrol fonksiyonunu ayarla (auth gerektiren handler'lardan once cagrilir)
    void setAuthChecker(AuthChecker checker) { authChecker_ = std::move(checker); }

    // Register a GET endpoint — stores metadata AND registers httplib route in one call
    // Bir GET endpoint'i kaydet — tek cagriyla hem metadata saklar hem httplib rotasi olusturur
    void get(httplib::Server* srv,
             const std::string& path,
             const std::string& desc,
             bool auth,
             httplib::Server::Handler handler,
             json params = json::object());

    // Register a POST endpoint — stores metadata AND registers httplib route in one call
    // Bir POST endpoint'i kaydet — tek cagriyla hem metadata saklar hem httplib rotasi olusturur
    void post(httplib::Server* srv,
              const std::string& path,
              const std::string& desc,
              bool auth,
              httplib::Server::Handler handler,
              json body = json::object());

    // Serialize all registered endpoints to JSON array
    // Tum kayitli endpoint'leri JSON dizisine serile et
    json toJson() const;

    // Get count of registered endpoints
    // Kayitli endpoint sayisini al
    size_t count() const { return endpoints_.size(); }

    // Get all endpoint info objects
    // Tum endpoint bilgi nesnelerini al
    const std::vector<EndpointInfo>& endpoints() const { return endpoints_; }

private:
    // Wrap a handler with auth check if required
    // Gerekirse handler'i auth kontrolu ile sar
    httplib::Server::Handler wrapAuth(bool auth, httplib::Server::Handler handler) const;

    std::vector<EndpointInfo> endpoints_;  // All registered endpoints / Tum kayitli endpoint'ler
    AuthChecker authChecker_;              // Auth checker function / Kimlik dogrulama fonksiyonu
};
