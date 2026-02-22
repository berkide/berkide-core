// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "BufferOptions.h"
#include <algorithm>

// Initialize with common editor defaults
// Yaygin editor varsayilanlariyla baslat
BufferOptions::BufferOptions() {
    defaults_["tabWidth"]     = 4;
    defaults_["shiftWidth"]   = 4;
    defaults_["useTabs"]      = false;
    defaults_["expandTab"]    = true;
    defaults_["encoding"]     = std::string("utf-8");
    defaults_["lineEnding"]   = std::string("lf");
    defaults_["trimTrailing"] = false;
    defaults_["insertFinalNewline"] = true;
    defaults_["readonly"]     = false;
    defaults_["wordWrap"]     = false;
    defaults_["wrapColumn"]   = 80;
    defaults_["scrollOff"]    = 5;
    defaults_["autoIndent"]   = true;
}

// Set a global default option value
// Global varsayilan secenek degerini ayarla
void BufferOptions::setDefault(const std::string& key, const OptionValue& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    defaults_[key] = value;
}

// Get a global default option value
// Global varsayilan secenek degerini al
std::optional<OptionValue> BufferOptions::getDefault(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = defaults_.find(key);
    if (it != defaults_.end()) return it->second;
    return std::nullopt;
}

// Set a buffer-local option (overrides global default for this buffer)
// Buffer-yerel secenegi ayarla (bu buffer icin global varsayilani gecersiz kilar)
void BufferOptions::setLocal(int bufferId, const std::string& key, const OptionValue& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    locals_[bufferId][key] = value;
}

// Remove a buffer-local option (falls back to global default)
// Buffer-yerel secenegi kaldir (global varsayilana geri doner)
void BufferOptions::removeLocal(int bufferId, const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = locals_.find(bufferId);
    if (it != locals_.end()) {
        it->second.erase(key);
        if (it->second.empty()) {
            locals_.erase(it);
        }
    }
}

// Get effective option: buffer-local first, then global default
// Gecerli secenegi al: once buffer-yerel, sonra global varsayilan
std::optional<OptionValue> BufferOptions::get(int bufferId, const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check buffer-local first
    // Once buffer-yerel kontrol et
    auto bufIt = locals_.find(bufferId);
    if (bufIt != locals_.end()) {
        auto keyIt = bufIt->second.find(key);
        if (keyIt != bufIt->second.end()) {
            return keyIt->second;
        }
    }

    // Fall back to global default
    // Global varsayilana geri don
    auto defIt = defaults_.find(key);
    if (defIt != defaults_.end()) return defIt->second;

    return std::nullopt;
}

// Check if a buffer has a local override for a key
// Bir buffer'in bir anahtar icin yerel gecersiz kilma degeri olup olmadigini kontrol et
bool BufferOptions::hasLocal(int bufferId, const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto bufIt = locals_.find(bufferId);
    if (bufIt == locals_.end()) return false;
    return bufIt->second.count(key) > 0;
}

// List all option keys for a buffer (merged: local + global defaults)
// Bir buffer icin tum secenek anahtarlarini listele (birlesmis: yerel + global varsayilanlar)
std::vector<std::string> BufferOptions::listKeys(int bufferId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_map<std::string, bool> seen;

    // Add buffer-local keys
    // Buffer-yerel anahtarlari ekle
    auto bufIt = locals_.find(bufferId);
    if (bufIt != locals_.end()) {
        for (auto& [k, _] : bufIt->second) seen[k] = true;
    }

    // Add global default keys
    // Global varsayilan anahtarlari ekle
    for (auto& [k, _] : defaults_) seen[k] = true;

    std::vector<std::string> keys;
    keys.reserve(seen.size());
    for (auto& [k, _] : seen) keys.push_back(k);
    std::sort(keys.begin(), keys.end());
    return keys;
}

// List all buffer-local override keys for a buffer
// Bir buffer icin tum buffer-yerel gecersiz kilma anahtarlarini listele
std::vector<std::string> BufferOptions::listLocalKeys(int bufferId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> keys;
    auto bufIt = locals_.find(bufferId);
    if (bufIt != locals_.end()) {
        keys.reserve(bufIt->second.size());
        for (auto& [k, _] : bufIt->second) keys.push_back(k);
        std::sort(keys.begin(), keys.end());
    }
    return keys;
}

// List all global default keys
// Tum global varsayilan anahtarlari listele
std::vector<std::string> BufferOptions::listDefaultKeys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> keys;
    keys.reserve(defaults_.size());
    for (auto& [k, _] : defaults_) keys.push_back(k);
    std::sort(keys.begin(), keys.end());
    return keys;
}

// Clear all local options for a buffer (e.g., when buffer is closed)
// Bir buffer icin tum yerel secenekleri temizle (ornegin buffer kapatildiginda)
void BufferOptions::clearBuffer(int bufferId) {
    std::lock_guard<std::mutex> lock(mutex_);
    locals_.erase(bufferId);
}

// Clear everything (all defaults and all buffer-local options)
// Her seyi temizle (tum varsayilanlar ve tum buffer-yerel secenekler)
void BufferOptions::clearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    defaults_.clear();
    locals_.clear();
}

// Get an option as int (returns fallback if not set or wrong type)
// Secenegi int olarak al (ayarlanmamissa veya yanlis tipse fallback dondurur)
int BufferOptions::getInt(int bufferId, const std::string& key, int fallback) const {
    auto val = get(bufferId, key);
    if (!val) return fallback;
    if (auto* p = std::get_if<int>(&*val)) return *p;
    return fallback;
}

// Get an option as bool
// Secenegi bool olarak al
bool BufferOptions::getBool(int bufferId, const std::string& key, bool fallback) const {
    auto val = get(bufferId, key);
    if (!val) return fallback;
    if (auto* p = std::get_if<bool>(&*val)) return *p;
    return fallback;
}

// Get an option as double
// Secenegi double olarak al
double BufferOptions::getDouble(int bufferId, const std::string& key, double fallback) const {
    auto val = get(bufferId, key);
    if (!val) return fallback;
    if (auto* p = std::get_if<double>(&*val)) return *p;
    return fallback;
}

// Get an option as string
// Secenegi string olarak al
std::string BufferOptions::getString(int bufferId, const std::string& key, const std::string& fallback) const {
    auto val = get(bufferId, key);
    if (!val) return fallback;
    if (auto* p = std::get_if<std::string>(&*val)) return *p;
    return fallback;
}
