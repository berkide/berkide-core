// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include <mutex>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// JSONC-based configuration system with layered priority:
//   hardcoded default -> app config -> user config -> CLI argument
// Katmanli oncelikli JSONC tabanli yapilandirma sistemi:
//   sabit varsayilan -> uygulama config -> kullanici config -> CLI arguman
class Config {
public:
    // Singleton access
    // Tekil erisim
    static Config& instance();

    // Load a JSONC file and deep-merge into current config.
    // Bir JSONC dosyasini yukle ve mevcut config'e derin birlestir.
    // Call multiple times for layered loading (app defaults first, then user override).
    // Katmanli yukleme icin birden fazla kez cagir (once uygulama varsayilanlari, sonra kullanici gecersiz kilma).
    bool loadFile(const std::string& path);

    // Apply CLI arguments as highest-priority overrides.
    // CLI argumanlarini en yuksek oncelikli gecersiz kilma olarak uygula.
    void applyCliArgs(int argc, char* argv[]);

    // Dot-notation accessors: "server.http_port", "log.level", etc.
    // Nokta notasyonu erisimcileri: "server.http_port", "log.level", vb.
    std::string getString(const std::string& key, const std::string& defaultVal = "") const;
    int         getInt(const std::string& key, int defaultVal = 0) const;
    bool        getBool(const std::string& key, bool defaultVal = false) const;

    // Return the entire merged config as JSON (for ConfigBinding)
    // Tum birlestirilmis config'i JSON olarak dondur (ConfigBinding icin)
    json raw() const;

private:
    Config();

    // Strip // and /* */ comments from JSONC input, respecting string literals.
    // JSONC girdisinden // ve /* */ yorumlarini temizle, string literallere dokunma.
    static std::string stripComments(const std::string& input);

    // Recursively merge 'override' into 'base'. Override values win.
    // 'override'i 'base'e rekursif olarak birlestir. Override degerleri kazanir.
    static void deepMerge(json& base, const json& override);

    // Resolve a dot-notation key ("a.b.c") to a json pointer.
    // Nokta notasyonu anahtarini ("a.b.c") json pointer'a cozumle.
    const json* resolve(const std::string& key) const;

    mutable std::mutex mutex_;
    json data_;    // Merged config data / Birlestirilmis config verisi
};
