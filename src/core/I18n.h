// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// Thread-safe internationalization (i18n) system with flat key namespace.
// Duz anahtar ad alaniyla thread-safe uluslararasilastirma (i18n) sistemi.
// Supports {{param}} substitution and locale fallback chain: requested -> "en" -> raw key.
// {{param}} degistirme ve yerel ayar geri donus zincirini destekler: istenen -> "en" -> ham anahtar.
class I18n {
public:
    // Get singleton instance
    // Tekil ornegi al
    static I18n& instance();

    // Set the active locale (e.g. "tr", "en", "de")
    // Aktif yerel ayari belirle (ornegin "tr", "en", "de")
    void setLocale(const std::string& locale);

    // Get the active locale
    // Aktif yerel ayari al
    std::string locale() const;

    // Load translations from a JSON file for a given locale
    // Belirli bir yerel ayar icin JSON dosyasindan cevirileri yukle
    bool loadLocaleFile(const std::string& locale, const std::string& path);

    // Register translation keys at runtime (e.g. from JS plugins)
    // Calisma zamaninda ceviri anahtarlarini kaydet (ornegin JS eklentilerinden)
    void registerKeys(const std::string& locale, const json& keys);

    // Translate a key with optional parameter substitution
    // Istege bagli parametre degistirme ile bir anahtari cevir
    // Fallback: locale -> "en" -> raw key
    // Geri donus: yerel ayar -> "en" -> ham anahtar
    std::string t(const std::string& key,
                  const std::unordered_map<std::string, std::string>& params = {}) const;

    // Check if a key exists in the current locale or fallback
    // Anahtarin mevcut yerel ayarda veya geri donuste var olup olmadigini kontrol et
    bool has(const std::string& key) const;

    // Get list of all loaded locales
    // Tum yuklenmis yerel ayarlarin listesini al
    std::vector<std::string> locales() const;

    // Get all keys for a specific locale
    // Belirli bir yerel ayar icin tum anahtarlari al
    std::vector<std::string> keys(const std::string& locale) const;

private:
    I18n() = default;

    // Lookup a key in a specific locale (no fallback)
    // Belirli bir yerel ayarda anahtar ara (geri donus yok)
    std::string lookup(const std::string& locale, const std::string& key) const;

    // Replace {{param}} placeholders in a template string
    // Sablon dizesindeki {{param}} yer tutucularini degistir
    std::string substitute(const std::string& tmpl,
                           const std::unordered_map<std::string, std::string>& params) const;

    mutable std::mutex mutex_;                                           // Thread safety / Thread guvenligi
    std::string locale_ = "en";                                          // Active locale / Aktif yerel ayar
    std::unordered_map<std::string,                                      // locale -> (key -> translation)
        std::unordered_map<std::string, std::string>> translations_;     // yerel ayar -> (anahtar -> ceviri)
};
