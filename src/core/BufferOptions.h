// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <variant>
#include <vector>
#include <optional>

// A single option value that can be int, bool, double, or string.
// Bir tek secenek degeri: int, bool, double veya string olabilir.
using OptionValue = std::variant<int, bool, double, std::string>;

// Per-buffer options storage with global defaults.
// Global varsayilanlarla buffer-bazli secenek depolama.
// Like Vim's :setlocal or Emacs buffer-local variables.
// Vim'in :setlocal veya Emacs'in buffer-local degiskenleri gibi.
class BufferOptions {
public:
    BufferOptions();

    // Set a global default option value
    // Global varsayilan secenek degerini ayarla
    void setDefault(const std::string& key, const OptionValue& value);

    // Get a global default option value
    // Global varsayilan secenek degerini al
    std::optional<OptionValue> getDefault(const std::string& key) const;

    // Set a buffer-local option (overrides global default for this buffer)
    // Buffer-yerel secenegi ayarla (bu buffer icin global varsayilani gecersiz kilar)
    void setLocal(int bufferId, const std::string& key, const OptionValue& value);

    // Remove a buffer-local option (falls back to global default)
    // Buffer-yerel secenegi kaldir (global varsayilana geri doner)
    void removeLocal(int bufferId, const std::string& key);

    // Get effective option value: buffer-local if set, otherwise global default
    // Gecerli secenek degerini al: ayarlanmissa buffer-yerel, yoksa global varsayilan
    std::optional<OptionValue> get(int bufferId, const std::string& key) const;

    // Check if a buffer has a local override for a key
    // Bir buffer'in bir anahtar icin yerel gecersiz kilma degeri olup olmadigini kontrol et
    bool hasLocal(int bufferId, const std::string& key) const;

    // List all option keys for a buffer (both local and global defaults)
    // Bir buffer icin tum secenek anahtarlarini listele (hem yerel hem global varsayilanlar)
    std::vector<std::string> listKeys(int bufferId) const;

    // List all buffer-local overrides for a buffer
    // Bir buffer icin tum buffer-yerel gecersiz kilmalari listele
    std::vector<std::string> listLocalKeys(int bufferId) const;

    // List all global default keys
    // Tum global varsayilan anahtarlari listele
    std::vector<std::string> listDefaultKeys() const;

    // Clear all local options for a buffer (e.g., when buffer is closed)
    // Bir buffer icin tum yerel secenekleri temizle (ornegin buffer kapatildiginda)
    void clearBuffer(int bufferId);

    // Clear everything (all defaults and all buffer-local options)
    // Her seyi temizle (tum varsayilanlar ve tum buffer-yerel secenekler)
    void clearAll();

    // === Type-safe convenience helpers ===
    // === Tip-guvenli kolaylik yardimcilari ===

    // Get an option as int (returns fallback if not set or wrong type)
    // Secenegi int olarak al (ayarlanmamissa veya yanlis tipse fallback dondurur)
    int getInt(int bufferId, const std::string& key, int fallback = 0) const;

    // Get an option as bool
    // Secenegi bool olarak al
    bool getBool(int bufferId, const std::string& key, bool fallback = false) const;

    // Get an option as double
    // Secenegi double olarak al
    double getDouble(int bufferId, const std::string& key, double fallback = 0.0) const;

    // Get an option as string
    // Secenegi string olarak al
    std::string getString(int bufferId, const std::string& key, const std::string& fallback = "") const;

private:
    mutable std::mutex mutex_;

    // Global defaults: key -> value
    // Global varsayilanlar: anahtar -> deger
    std::unordered_map<std::string, OptionValue> defaults_;

    // Per-buffer overrides: bufferId -> (key -> value)
    // Buffer-bazli gecersiz kilmalar: bufferId -> (anahtar -> deger)
    std::unordered_map<int, std::unordered_map<std::string, OptionValue>> locals_;
};
