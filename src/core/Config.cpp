// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "Config.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>

// Singleton instance accessor
// Tekil ornek erisimcisi
Config& Config::instance() {
    static Config inst;
    return inst;
}

// Initialize with hardcoded defaults (lowest priority layer)
// Sabit varsayilanlarla baslat (en dusuk oncelik katmani)
Config::Config() {
    data_ = {
        {"server", {
            {"http_port", 1881},
            {"ws_port", 1882},
            {"bind_address", "127.0.0.1"},
            {"token", ""},
            {"tls", {
                {"enabled", false},
                {"cert", ""},
                {"key", ""},
                {"ca", "NONE"}
            }}
        }},
        {"editor", {
            {"tab_width", 4},
            {"shift_width", 4},
            {"use_tabs", false},
            {"line_numbers", true},
            {"word_wrap", false},
            {"extra_word_chars", "_"}
        }},
        {"completion", {
            {"max_results", 50},
            {"auto_trigger", true}
        }},
        {"search", {
            {"case_sensitive", true},
            {"regex", false},
            {"whole_word", false},
            {"wrap_around", true}
        }},
        {"autosave", {
            {"enabled", true},
            {"interval", 30}
        }},
        {"session", {
            {"enabled", true},
            {"restore_on_start", true}
        }},
        {"window", {
            {"width", 80},
            {"height", 24},
            {"split_ratio", 0.5}
        }},
        {"inspector", {
            {"enabled", false},
            {"port", 9229},
            {"break_on_start", false}
        }},
        {"log", {
            {"level", "info"},
            {"file", false},
            {"path", "logs"}
        }},
        {"locale", "en"},
        {"diff", {
            {"context_lines", 3}
        }},
        {"fold", {
            {"default_collapsed", false}
        }},
        {"indent", {
            {"auto", true}
        }},
        {"plugins", {
            {"enabled", true},
            {"watch", true}
        }},
        {"treesitter", {
            {"enabled", true}
        }}
    };
}

// Load JSONC file, strip comments, parse, and deep-merge into current config
// JSONC dosyasini yukle, yorumlari temizle, ayristir ve mevcut config'e derin birlestir
bool Config::loadFile(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        LOG_DEBUG("[Config] File not found (skipping): ", path);
        return false;
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            LOG_ERROR("[Config] Cannot open: ", path);
            return false;
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        std::string raw = ss.str();

        std::string cleaned = stripComments(raw);
        json parsed = json::parse(cleaned);

        if (!parsed.is_object()) {
            LOG_ERROR("[Config] Not a JSON object: ", path);
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        deepMerge(data_, parsed);
        LOG_INFO("[Config] Loaded: ", path);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("[Config] Parse error in ", path, ": ", e.what());
        return false;
    }
}

// Apply CLI arguments as highest-priority overrides
// CLI argumanlarini en yuksek oncelikli gecersiz kilma olarak uygula
void Config::applyCliArgs(int argc, char* argv[]) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--remote") {
            data_["server"]["bind_address"] = "0.0.0.0";
        } else if (arg.rfind("--http-port=", 0) == 0) {
            data_["server"]["http_port"] = std::stoi(arg.substr(12));
        } else if (arg.rfind("--ws-port=", 0) == 0) {
            data_["server"]["ws_port"] = std::stoi(arg.substr(10));
        } else if (arg.rfind("--port=", 0) == 0) {
            data_["server"]["http_port"] = std::stoi(arg.substr(7));
        } else if (arg.rfind("--token=", 0) == 0) {
            data_["server"]["token"] = arg.substr(8);
        } else if (arg.rfind("--tls-cert=", 0) == 0) {
            data_["server"]["tls"]["cert"] = arg.substr(11);
            data_["server"]["tls"]["enabled"] = true;
        } else if (arg.rfind("--tls-key=", 0) == 0) {
            data_["server"]["tls"]["key"] = arg.substr(10);
            data_["server"]["tls"]["enabled"] = true;
        } else if (arg.rfind("--tls-ca=", 0) == 0) {
            data_["server"]["tls"]["ca"] = arg.substr(9);
        } else if (arg == "--inspect") {
            data_["inspector"]["enabled"] = true;
        } else if (arg == "--inspect-brk") {
            data_["inspector"]["enabled"] = true;
            data_["inspector"]["break_on_start"] = true;
        } else if (arg.rfind("--inspect-port=", 0) == 0) {
            data_["inspector"]["port"] = std::stoi(arg.substr(15));
            data_["inspector"]["enabled"] = true;
        } else if (arg.rfind("--locale=", 0) == 0) {
            data_["locale"] = arg.substr(9);
        }
    }
}

// Get a string value by dot-notation key
// Nokta notasyonu anahtariyla string deger al
std::string Config::getString(const std::string& key, const std::string& defaultVal) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const json* node = resolve(key);
    if (!node || !node->is_string()) return defaultVal;
    return node->get<std::string>();
}

// Get an int value by dot-notation key
// Nokta notasyonu anahtariyla int deger al
int Config::getInt(const std::string& key, int defaultVal) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const json* node = resolve(key);
    if (!node || !node->is_number_integer()) return defaultVal;
    return node->get<int>();
}

// Get a bool value by dot-notation key
// Nokta notasyonu anahtariyla bool deger al
bool Config::getBool(const std::string& key, bool defaultVal) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const json* node = resolve(key);
    if (!node || !node->is_boolean()) return defaultVal;
    return node->get<bool>();
}

// Return the full merged config JSON
// Tam birlestirilmis config JSON'unu dondur
json Config::raw() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
}

// Strip single-line (//) and multi-line (/* */) comments from JSONC,
// leaving string literals untouched.
// JSONC'den tek satirlik (//) ve cok satirlik (/* */) yorumlari temizle,
// string literallere dokunma.
std::string Config::stripComments(const std::string& input) {
    std::string out;
    out.reserve(input.size());

    size_t i = 0;
    const size_t len = input.size();

    while (i < len) {
        // String literal — copy as-is, handling escape sequences
        // String literal — oldugu gibi kopyala, kacis dizilerini isle
        if (input[i] == '"') {
            out += '"';
            ++i;
            while (i < len && input[i] != '"') {
                if (input[i] == '\\' && i + 1 < len) {
                    out += input[i];
                    out += input[i + 1];
                    i += 2;
                } else {
                    out += input[i];
                    ++i;
                }
            }
            if (i < len) {
                out += '"';
                ++i;
            }
        }
        // Single-line comment — skip to end of line
        // Tek satirlik yorum — satir sonuna kadar atla
        else if (i + 1 < len && input[i] == '/' && input[i + 1] == '/') {
            while (i < len && input[i] != '\n') ++i;
        }
        // Multi-line comment — skip to closing */
        // Cok satirlik yorum — kapatan */ a kadar atla
        else if (i + 1 < len && input[i] == '/' && input[i + 1] == '*') {
            i += 2;
            while (i + 1 < len && !(input[i] == '*' && input[i + 1] == '/')) ++i;
            if (i + 1 < len) i += 2;
        }
        // Normal character — copy
        // Normal karakter — kopyala
        else {
            out += input[i];
            ++i;
        }
    }
    return out;
}

// Recursively merge 'override' into 'base'. Objects are merged, scalars are replaced.
// 'override'i 'base'e rekursif olarak birlestir. Nesneler birlestir, skalerler degistir.
void Config::deepMerge(json& base, const json& override) {
    for (auto& [key, val] : override.items()) {
        if (val.is_object() && base.contains(key) && base[key].is_object()) {
            deepMerge(base[key], val);
        } else {
            base[key] = val;
        }
    }
}

// Resolve dot-notation key ("server.http_port") to a JSON node pointer
// Nokta notasyonu anahtarini ("server.http_port") JSON dugum pointer'ina cozumle
const json* Config::resolve(const std::string& key) const {
    const json* node = &data_;
    std::istringstream ss(key);
    std::string part;

    while (std::getline(ss, part, '.')) {
        if (!node->is_object() || !node->contains(part)) return nullptr;
        node = &(*node)[part];
    }
    return node;
}
