#include "PluginManager.h"
#include "V8Engine.h"
#include "Logger.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stack>
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

// Discover plugins in a directory: reads plugin.json manifests or creates synthetic ones for loose files
// Bir dizindeki eklentileri kesfet: plugin.json manifestlerini oku veya gevrek dosyalar icin sentetik olustur
void PluginManager::discover(const std::string& pluginDir) {
    if (!fs::exists(pluginDir)) return;

    for (const auto& entry : fs::directory_iterator(pluginDir)) {
        if (entry.is_directory()) {
            // Look for plugin.json in subdirectory
            // Alt dizinde plugin.json ara
            fs::path manifestPath = entry.path() / "plugin.json";
            if (fs::exists(manifestPath)) {
                std::ifstream ifs(manifestPath);
                json j = json::parse(ifs, nullptr, false);
                if (j.is_discarded()) {
                    LOG_WARN("[Plugin] Invalid plugin.json: ", manifestPath.string());
                    continue;
                }

                PluginState ps;
                ps.manifest.name        = j.value("name", entry.path().filename().string());
                ps.manifest.version     = j.value("version", "0.0.1");
                ps.manifest.description = j.value("description", "");
                ps.manifest.main        = j.value("main", "index.js");
                ps.manifest.enabled     = j.value("enabled", true);
                if (j.contains("dependencies") && j["dependencies"].is_array()) {
                    for (auto& dep : j["dependencies"]) {
                        ps.manifest.dependencies.push_back(dep.get<std::string>());
                    }
                }
                ps.dirPath = entry.path().string();

                nameIndex_[ps.manifest.name] = plugins_.size();
                plugins_.push_back(std::move(ps));
                LOG_INFO("[Plugin] Discovered: ", ps.manifest.name);
            }
        } else if (entry.is_regular_file()) {
            // Single file plugin (synthetic manifest)
            // Tek dosya eklentisi (sentetik manifest)
            auto ext = entry.path().extension().string();
            if (ext != ".js" && ext != ".mjs") continue;

            PluginState ps;
            ps.manifest.name = entry.path().stem().string();
            ps.manifest.main = entry.path().filename().string();
            ps.dirPath = entry.path().parent_path().string();

            // Skip if already discovered with same name
            // Ayni isimle zaten kesfedildiyse atla
            if (nameIndex_.count(ps.manifest.name)) continue;

            nameIndex_[ps.manifest.name] = plugins_.size();
            plugins_.push_back(std::move(ps));
            LOG_INFO("[Plugin] Discovered (single file): ", ps.manifest.name);
        }
    }

    LOG_INFO("[Plugin] Discovery complete: ", plugins_.size(), " plugins found");
}

// Topological sort: returns plugin indices in dependency order
// Topolojik siralama: eklenti indekslerini bagimlilik sirasinda dondurur
std::vector<size_t> PluginManager::topologicalSort() {
    size_t n = plugins_.size();
    std::unordered_map<size_t, std::vector<size_t>> adj;
    std::vector<int> inDeg(n, 0);

    for (size_t i = 0; i < n; ++i) {
        for (auto& dep : plugins_[i].manifest.dependencies) {
            auto it = nameIndex_.find(dep);
            if (it != nameIndex_.end()) {
                adj[it->second].push_back(i);
                inDeg[i]++;
            }
        }
    }

    // Kahn's algorithm
    std::vector<size_t> order;
    std::queue<size_t> q;
    for (size_t i = 0; i < n; ++i) {
        if (inDeg[i] == 0) q.push(i);
    }

    while (!q.empty()) {
        size_t u = q.front(); q.pop();
        order.push_back(u);
        for (auto v : adj[u]) {
            if (--inDeg[v] == 0) q.push(v);
        }
    }

    if (order.size() != n) {
        LOG_WARN("[Plugin] Circular dependency detected, loading remaining in order");
        for (size_t i = 0; i < n; ++i) {
            if (std::find(order.begin(), order.end(), i) == order.end()) {
                order.push_back(i);
            }
        }
    }

    return order;
}

// Load a single plugin using V8Engine
// V8Engine kullanarak tek bir eklentiyi yukle
bool PluginManager::loadPlugin(PluginState& ps) {
    if (!engine_) {
        ps.hasError = true;
        ps.error = "No V8 engine";
        return false;
    }

    fs::path entryPath = fs::path(ps.dirPath) / ps.manifest.main;
    if (!fs::exists(entryPath)) {
        ps.hasError = true;
        ps.error = "Entry file not found: " + entryPath.string();
        LOG_ERROR("[Plugin] ", ps.error);
        return false;
    }

    bool ok = false;
    auto ext = entryPath.extension().string();
    if (ext == ".mjs") {
        ok = engine_->loadModule(entryPath.string());
    } else {
        ok = engine_->loadScriptFromFile(entryPath.string());
    }

    if (ok) {
        ps.loaded = true;
        ps.hasError = false;
        ps.error.clear();
        LOG_INFO("[Plugin] Loaded: ", ps.manifest.name);
    } else {
        ps.hasError = true;
        ps.error = "Failed to load entry file";
        LOG_ERROR("[Plugin] Failed to load: ", ps.manifest.name);
    }

    return ok;
}

// Load all discovered and enabled plugins in topological (dependency) order
// Kesfedilen ve etkinlestirilen tum eklentileri topolojik (bagimlilik) sirasinda yukle
void PluginManager::loadAll() {
    auto order = topologicalSort();

    int loaded = 0;
    for (size_t idx : order) {
        auto& ps = plugins_[idx];
        if (!ps.manifest.enabled) {
            LOG_INFO("[Plugin] Skipping disabled: ", ps.manifest.name);
            continue;
        }
        if (ps.loaded) continue;

        if (loadPlugin(ps)) ++loaded;
    }

    LOG_INFO("[Plugin] ", loaded, " plugins loaded");
}

// Activate a plugin by name
// Ismiyle bir eklentiyi etkinlestir
bool PluginManager::activate(const std::string& name) {
    auto* ps = find(name);
    if (!ps) return false;
    ps->manifest.enabled = true;
    if (!ps->loaded) return loadPlugin(*ps);
    return true;
}

// Deactivate a plugin by name (marks as unloaded, actual cleanup is JS-side)
// Ismiyle bir eklentiyi devre disi birak (yuklenmemis olarak isaretler, gercek temizlik JS tarafinda)
bool PluginManager::deactivate(const std::string& name) {
    auto* ps = find(name);
    if (!ps) return false;
    ps->loaded = false;
    LOG_INFO("[Plugin] Deactivated: ", name);
    return true;
}

// Enable a plugin for loading
// Bir eklentiyi yukleme icin etkinlestir
bool PluginManager::enable(const std::string& name) {
    auto* ps = find(name);
    if (!ps) return false;
    ps->manifest.enabled = true;
    LOG_INFO("[Plugin] Enabled: ", name);
    return true;
}

// Disable a plugin
// Bir eklentiyi devre disi birak
bool PluginManager::disable(const std::string& name) {
    auto* ps = find(name);
    if (!ps) return false;
    ps->manifest.enabled = false;
    ps->loaded = false;
    LOG_INFO("[Plugin] Disabled: ", name);
    return true;
}

// Find plugin by name, returns nullptr if not found
// Ismiyle eklenti bul, bulunamazsa nullptr dondur
PluginState* PluginManager::find(const std::string& name) {
    auto it = nameIndex_.find(name);
    if (it == nameIndex_.end()) return nullptr;
    return &plugins_[it->second];
}
