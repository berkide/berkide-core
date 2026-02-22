// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <vector>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// Thread-safe command registry that maps command names to handler functions.
// Komut adlarini isleyici fonksiyonlara eslestiren thread-safe komut kaydedicisi.
// Supports both void commands (mutations) and json-returning queries.
// Hem void komutlari (mutasyonlar) hem de json donduren sorgulari destekler.
class CommandRegistry {
public:
    // Mutation command: performs action, returns no data
    // Mutasyon komutu: islem yapar, veri dondurmez
    using CommandFn = std::function<void(const json&)>;

    // Query command: performs read, returns JSON data
    // Sorgu komutu: okuma yapar, JSON verisi dondurur
    using QueryFn = std::function<json(const json&)>;

    // Register a mutation command with a unique name
    // Benzersiz bir isimle mutasyon komutu kaydet
    bool registerCommand(const std::string& name, CommandFn fn);

    // Register a query command that returns JSON data
    // JSON verisi donduren bir sorgu komutu kaydet
    bool registerQuery(const std::string& name, QueryFn fn);

    // Execute a command by name (backward compat, returns found/not-found)
    // Komutu ismiyle calistir (geri uyumluluk, bulundu/bulunamadi dondurur)
    bool execute(const std::string& name, const json& args = {});

    // Execute and return full result with data (for queries)
    // Calistir ve veriyle tam sonuc dondur (sorgular icin)
    json executeWithResult(const std::string& name, const json& args = {});

    // Check if a command or query exists in the registry
    // Kayit defterinde bir komut veya sorgunun var olup olmadigini kontrol et
    bool exists(const std::string& name) const;

    // List all registered command and query names
    // Tum kayitli komut ve sorgu adlarini listele
    json listAll() const;

private:
    mutable std::mutex mutex_;                               // Thread safety lock / Thread guvenligi kilidi
    std::unordered_map<std::string, CommandFn> commands_;    // Mutation handlers / Mutasyon isleyicileri
    std::unordered_map<std::string, QueryFn> queries_;       // Query handlers / Sorgu isleyicileri
};
