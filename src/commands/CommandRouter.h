// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include "CommandRegistry.h"
#include <string>
#include <memory>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// High-level command dispatcher that wraps CommandRegistry.
// CommandRegistry'yi saran ust duzey komut dagitcisi.
// Provides both C++ and JS interfaces for command execution.
// Komut yurutme icin hem C++ hem JS arayuzleri saglar.
class CommandRouter {
public:
    CommandRouter();
    ~CommandRouter() = default;

    // Register a native C++ mutation command handler
    // Yerel C++ mutasyon komut isleyicisi kaydet
    void registerNative(const std::string& name, CommandRegistry::CommandFn fn);

    // Register a native C++ query command that returns JSON data
    // JSON verisi donduren yerel C++ sorgu komutu kaydet
    void registerQuery(const std::string& name, CommandRegistry::QueryFn fn);

    // Execute a command by name from C++ side (backward compat)
    // C++ tarafindan ismiyle bir komutu calistir (geri uyumluluk)
    bool execute(const std::string& name, const json& args = {});

    // Execute and return full result with data (for HTTP/WS responses)
    // Calistir ve veriyle tam sonuc dondur (HTTP/WS yanitlari icin)
    json executeWithResult(const std::string& name, const json& args = {});

    // Execute a command from JavaScript (takes JSON string, returns result string)
    // JavaScript'ten bir komutu calistir (JSON dizesi alir, sonuc dizesi dondurur)
    std::string execFromJS(const std::string& name, const std::string& jsonArgs);

    // List all registered commands and queries
    // Tum kayitli komutlari ve sorgulari listele
    json listAll() const;

private:
    std::unique_ptr<CommandRegistry> registry_;  // Underlying command registry / Alttaki komut kaydedicisi
};
