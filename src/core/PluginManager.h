// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// Manifest data for a plugin (parsed from plugin.json or synthetic)
// Bir eklenti icin manifest verisi (plugin.json'dan ayristirilan veya sentetik)
struct PluginManifest {
    std::string name;
    std::string version = "0.0.1";
    std::string description;
    std::string main;                       // Entry point file / Giris noktasi dosyasi
    std::vector<std::string> dependencies;  // Plugin names this depends on / Bagimli oldugu eklenti isimleri
    bool enabled = true;
};

// Runtime state of a loaded plugin
// Yuklu bir eklentinin calisma zamani durumu
struct PluginState {
    PluginManifest manifest;
    std::string dirPath;    // Directory where the plugin lives / Eklentinin bulundugu dizin
    bool loaded  = false;
    bool hasError = false;
    std::string error;
};

class V8Engine;

// Manages plugin discovery, loading (topological sort), enable/disable lifecycle.
// Eklenti kesfi, yukleme (topolojik siralama), etkinlestirme/devre disi birakma yasam dongusunu yonetir.
class PluginManager {
public:
    PluginManager() = default;

    // Set the V8 engine for script loading
    // Betik yukleme icin V8 motorunu ayarla
    void setEngine(V8Engine* engine) { engine_ = engine; }

    // Discover plugins from a directory (looks for plugin.json or single .js/.mjs files)
    // Bir dizinden eklentileri kesfet (plugin.json veya tek .js/.mjs dosyalari arar)
    void discover(const std::string& pluginDir);

    // Load all discovered and enabled plugins in dependency order
    // Kesfedilen ve etkinlestirilen tum eklentileri bagimlilik sirasinda yukle
    void loadAll();

    // Activate a plugin by name (load if not loaded)
    // Ismiyle bir eklentiyi etkinlestir (yuklenmemisse yukle)
    bool activate(const std::string& name);

    // Deactivate a plugin by name
    // Ismiyle bir eklentiyi devre disi birak
    bool deactivate(const std::string& name);

    // Enable a plugin (mark for loading on next loadAll)
    // Bir eklentiyi etkinlestir (sonraki loadAll'da yuklenmek uzere isaretle)
    bool enable(const std::string& name);

    // Disable a plugin (mark as disabled, won't load)
    // Bir eklentiyi devre disi birak (devre disi olarak isaretle, yuklenmez)
    bool disable(const std::string& name);

    // Get list of all plugins
    // Tum eklentilerin listesini al
    const std::vector<PluginState>& list() const { return plugins_; }

    // Find plugin by name
    // Ismiyle eklenti bul
    PluginState* find(const std::string& name);

private:
    // Topological sort for dependency ordering
    // Bagimlilik siralaması icin topolojik siralama
    std::vector<size_t> topologicalSort();

    // Load a single plugin
    // Tek bir eklentiyi yukle
    bool loadPlugin(PluginState& ps);

    V8Engine* engine_ = nullptr;
    std::vector<PluginState> plugins_;
    std::unordered_map<std::string, size_t> nameIndex_;  // name -> index / isim -> indeks
};
