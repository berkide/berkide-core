// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "I18n.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>

// Singleton instance accessor
// Tekil ornek erisimcisi
I18n& I18n::instance() {
    static I18n inst;
    return inst;
}

// Set the active locale string
// Aktif yerel ayar dizesini ayarla
void I18n::setLocale(const std::string& locale) {
    std::lock_guard<std::mutex> lock(mutex_);
    locale_ = locale;
    LOG_INFO("[I18n] Locale set to: ", locale);
}

// Return the current active locale
// Mevcut aktif yerel ayari dondur
std::string I18n::locale() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return locale_;
}

// Load a JSON locale file and merge its keys into the translations map
// Bir JSON yerel ayar dosyasini yukle ve anahtarlarini ceviri haritasina birlestir
bool I18n::loadLocaleFile(const std::string& locale, const std::string& path) {
    if (!std::filesystem::exists(path)) {
        LOG_DEBUG("[I18n] Locale file not found: ", path);
        return false;
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            LOG_ERROR("[I18n] Cannot open locale file: ", path);
            return false;
        }

        json j = json::parse(file);
        if (!j.is_object()) {
            LOG_ERROR("[I18n] Locale file is not a JSON object: ", path);
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto& localeMap = translations_[locale];
        for (auto& [key, val] : j.items()) {
            if (val.is_string()) {
                localeMap[key] = val.get<std::string>();
            }
        }

        LOG_INFO("[I18n] Loaded ", localeMap.size(), " keys for locale '", locale, "' from ", path);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("[I18n] Failed to parse locale file: ", path, " (", e.what(), ")");
        return false;
    }
}

// Register translation keys from a JSON object at runtime
// Calisma zamaninda bir JSON nesnesinden ceviri anahtarlarini kaydet
void I18n::registerKeys(const std::string& locale, const json& keys) {
    if (!keys.is_object()) return;

    std::lock_guard<std::mutex> lock(mutex_);
    auto& localeMap = translations_[locale];
    for (auto& [key, val] : keys.items()) {
        if (val.is_string()) {
            localeMap[key] = val.get<std::string>();
        }
    }
    LOG_DEBUG("[I18n] Registered ", keys.size(), " keys for locale '", locale, "'");
}

// Translate a key with parameter substitution and fallback chain
// Parametre degistirme ve geri donus zinciri ile bir anahtari cevir
std::string I18n::t(const std::string& key,
                    const std::unordered_map<std::string, std::string>& params) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Try current locale first
    // Ilk olarak mevcut yerel ayari dene
    std::string result = lookup(locale_, key);

    // Fallback to English if not found
    // Bulunamazsa Ingilizce'ye geri don
    if (result.empty() && locale_ != "en") {
        result = lookup("en", key);
    }

    // Return raw key if nothing found
    // Hicbir sey bulunamazsa ham anahtari dondur
    if (result.empty()) {
        return substitute(key, params);
    }

    return substitute(result, params);
}

// Check if a key exists in the current locale or English fallback
// Anahtarin mevcut yerel ayarda veya Ingilizce geri donuste var olup olmadigini kontrol et
bool I18n::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!lookup(locale_, key).empty()) return true;
    if (locale_ != "en" && !lookup("en", key).empty()) return true;
    return false;
}

// Return list of all loaded locale identifiers
// Tum yuklenmis yerel ayar tanimlayicilarinin listesini dondur
std::vector<std::string> I18n::locales() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(translations_.size());
    for (const auto& [loc, _] : translations_) {
        result.push_back(loc);
    }
    return result;
}

// Return all translation keys for a specific locale
// Belirli bir yerel ayar icin tum ceviri anahtarlarini dondur
std::vector<std::string> I18n::keys(const std::string& locale) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    auto it = translations_.find(locale);
    if (it != translations_.end()) {
        result.reserve(it->second.size());
        for (const auto& [key, _] : it->second) {
            result.push_back(key);
        }
    }
    return result;
}

// Internal: look up a key in a specific locale without fallback
// Dahili: geri donus olmadan belirli bir yerel ayarda anahtar ara
std::string I18n::lookup(const std::string& locale, const std::string& key) const {
    auto locIt = translations_.find(locale);
    if (locIt == translations_.end()) return "";
    auto keyIt = locIt->second.find(key);
    if (keyIt == locIt->second.end()) return "";
    return keyIt->second;
}

// Internal: replace all {{param}} placeholders with values from params map
// Dahili: tum {{param}} yer tutucularini params haritasindaki degerlerle degistir
std::string I18n::substitute(const std::string& tmpl,
                             const std::unordered_map<std::string, std::string>& params) const {
    if (params.empty()) return tmpl;

    std::string result = tmpl;
    for (const auto& [key, val] : params) {
        std::string placeholder = "{{" + key + "}}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), val);
            pos += val.length();
        }
    }
    return result;
}
