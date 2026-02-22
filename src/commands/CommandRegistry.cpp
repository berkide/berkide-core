// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "CommandRegistry.h"
#include "ApiResponse.h"
#include "Logger.h"

// Register a named mutation command with its handler function (thread-safe)
// Adlandirilmis bir mutasyon komutunu isleyici fonksiyonuyla kaydet (is parcacigi guvenli)
bool CommandRegistry::registerCommand(const std::string& name, CommandFn fn) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (commands_.contains(name) || queries_.contains(name)) {
        LOG_WARN("[CommandRegistry] Command already registered: ", name);
        return false;
    }
    commands_[name] = std::move(fn);
    LOG_DEBUG("[CommandRegistry] Registered command: ", name);
    return true;
}

// Register a named query command that returns JSON data (thread-safe)
// JSON verisi donduren adlandirilmis bir sorgu komutunu kaydet (is parcacigi guvenli)
bool CommandRegistry::registerQuery(const std::string& name, QueryFn fn) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (commands_.contains(name) || queries_.contains(name)) {
        LOG_WARN("[CommandRegistry] Query already registered: ", name);
        return false;
    }
    queries_[name] = std::move(fn);
    LOG_DEBUG("[CommandRegistry] Registered query: ", name);
    return true;
}

// Execute a command by name, returns true if found and executed successfully
// Komutu ismiyle calistir, bulundu ve basariyla calistirildiysa true dondur
bool CommandRegistry::execute(const std::string& name, const json& args) {
    json result = executeWithResult(name, args);
    return result.value("ok", false);
}

// Execute command or query and return full JSON result in standard ApiResponse format
// Komut veya sorguyu calistir ve standart ApiResponse formatinda tam JSON sonucunu dondur
json CommandRegistry::executeWithResult(const std::string& name, const json& args) {
    // Try mutation commands first
    // Once mutasyon komutlarini dene
    {
        CommandFn fn;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = commands_.find(name);
            if (it != commands_.end()) {
                fn = it->second;
            }
        }
        if (fn) {
            try {
                fn(args);
                return ApiResponse::ok(true);
            } catch (const std::exception& e) {
                LOG_ERROR("[CommandRegistry] Execution error in '", name, "': ", e.what());
                return ApiResponse::error("COMMAND_ERROR", "command.error",
                    {{"name", name}, {"error", e.what()}});
            }
        }
    }

    // Try query commands
    // Sorgu komutlarini dene
    {
        QueryFn fn;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = queries_.find(name);
            if (it != queries_.end()) {
                fn = it->second;
            }
        }
        if (fn) {
            try {
                json data = fn(args);
                return ApiResponse::ok(data);
            } catch (const std::exception& e) {
                LOG_ERROR("[CommandRegistry] Query error in '", name, "': ", e.what());
                return ApiResponse::error("QUERY_ERROR", "command.error",
                    {{"name", name}, {"error", e.what()}});
            }
        }
    }

    return ApiResponse::error("NOT_FOUND", "command.not_found", {{"name", name}});
}

// Check whether a command or query with the given name is registered
// Verilen isimde bir komut veya sorgunun kayitli olup olmadigini kontrol et
bool CommandRegistry::exists(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return commands_.contains(name) || queries_.contains(name);
}

// List all registered commands and queries with their types
// Tum kayitli komutlari ve sorgulari turleriyle listele
json CommandRegistry::listAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    json result;
    json cmds = json::array();
    json qrys = json::array();
    for (const auto& [name, _] : commands_) {
        cmds.push_back(name);
    }
    for (const auto& [name, _] : queries_) {
        qrys.push_back(name);
    }
    result["commands"] = cmds;
    result["queries"] = qrys;
    result["totalCommands"] = commands_.size();
    result["totalQueries"] = queries_.size();
    result["total"] = commands_.size() + queries_.size();
    return result;
}
