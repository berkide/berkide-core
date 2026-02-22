// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "CommandRouter.h"

// Initialize the command router and create the internal command registry
// Komut yonlendiricisini baslat ve dahili komut kayit defterini olustur
CommandRouter::CommandRouter() {
    registry_ = std::make_unique<CommandRegistry>();
}

// Register a native C++ mutation command handler by name
// Ada gore yerel bir C++ mutasyon komut isleyicisi kaydet
void CommandRouter::registerNative(const std::string& name, CommandRegistry::CommandFn fn) {
    registry_->registerCommand(name, std::move(fn));
}

// Register a native C++ query command that returns JSON data
// JSON verisi donduren yerel C++ sorgu komutunu kaydet
void CommandRouter::registerQuery(const std::string& name, CommandRegistry::QueryFn fn) {
    registry_->registerQuery(name, std::move(fn));
}

// Execute a command by name with JSON arguments (backward compat, returns bool)
// JSON argumanlariyla komutu ada gore calistir (geri uyumluluk, bool dondurur)
bool CommandRouter::execute(const std::string& name, const json& args) {
    return registry_->execute(name, args);
}

// Execute and return full JSON result including query data
// Sorgu verisi dahil tam JSON sonucunu calistir ve dondur
json CommandRouter::executeWithResult(const std::string& name, const json& args) {
    return registry_->executeWithResult(name, args);
}

// Execute a command from JavaScript, parsing JSON string args and returning JSON result
// JavaScript'ten komut calistir, JSON dize argumanlarini ayristir ve JSON sonucu dondur
std::string CommandRouter::execFromJS(const std::string& name, const std::string& jsonArgs) {
    try {
        json args = json::parse(jsonArgs.empty() ? "{}" : jsonArgs);
        json result = registry_->executeWithResult(name, args);
        return result.dump();
    } catch (const std::exception& e) {
        return std::string(R"({"ok":false,"error":")") + e.what() + "\"}";
    }
}

// List all registered commands and queries
// Tum kayitli komutlari ve sorgulari listele
json CommandRouter::listAll() const {
    return registry_->listAll();
}
