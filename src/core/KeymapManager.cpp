#include "KeymapManager.h"
#include "Logger.h"

// Default constructor: create global keymap
// Varsayilan kurucu: global tus haritasini olustur
KeymapManager::KeymapManager() {
    keymaps_["global"] = {"global", "", {}};
    keymaps_["normal"] = {"normal", "global", {}};
    keymaps_["insert"] = {"insert", "global", {}};
    keymaps_["visual"] = {"visual", "normal", {}};
}

// Create or get a keymap with optional parent
// Istege bagli ust ile tus haritasi olustur veya al
void KeymapManager::createKeymap(const std::string& name, const std::string& parent) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (keymaps_.find(name) == keymaps_.end()) {
        keymaps_[name] = {name, parent, {}};
    }
}

// Set a key binding in a keymap
// Bir tus haritasinda tus baglantisi ayarla
void KeymapManager::set(const std::string& keymapName, const std::string& keys,
                         const std::string& command, const std::string& argsJson) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& km = keymaps_[keymapName];
    if (km.name.empty()) km.name = keymapName;

    // Replace existing binding if keys match
    // Tuslar eslesiyorsa mevcut baglantiyi degistir
    for (auto& b : km.bindings) {
        if (b.keys == keys) {
            b.command = command;
            b.argsJson = argsJson;
            return;
        }
    }

    km.bindings.push_back({keys, command, argsJson});
}

// Remove a key binding
// Tus baglantisini kaldir
bool KeymapManager::remove(const std::string& keymapName, const std::string& keys) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = keymaps_.find(keymapName);
    if (it == keymaps_.end()) return false;

    auto& bindings = it->second.bindings;
    for (auto bit = bindings.begin(); bit != bindings.end(); ++bit) {
        if (bit->keys == keys) {
            bindings.erase(bit);
            return true;
        }
    }
    return false;
}

// Lookup a key sequence in the keymap hierarchy (searches up parent chain)
// Tus haritasi hiyerarsisinde bir tus dizisini ara (ust zincirde arar)
const KeyBinding* KeymapManager::lookup(const std::string& keymapName,
                                         const std::string& keys) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string current = keymapName;

    // Walk up the parent chain
    // Ust zincirde yuru
    while (!current.empty()) {
        auto it = keymaps_.find(current);
        if (it == keymaps_.end()) break;

        for (auto& b : it->second.bindings) {
            if (b.keys == keys) return &b;
        }
        current = it->second.parent;
    }
    return nullptr;
}

// Feed a key press into the prefix state machine
// On ek durum makinesine bir tus basisi besle
std::string KeymapManager::feedKey(const std::string& keymapName, const std::string& key) {
    // Build the current full sequence
    // Mevcut tam diziyi olustur
    if (!pendingKeys_.empty()) {
        pendingKeys_ += " " + key;
    } else {
        pendingKeys_ = key;
    }

    // Check for exact match
    // Tam esleme kontrol et
    const KeyBinding* binding = lookup(keymapName, pendingKeys_);
    if (binding) {
        std::string cmd = binding->command;
        pendingKeys_.clear();
        return cmd;
    }

    // Check if any binding starts with our prefix
    // Herhangi bir baglantinin bizim on ekimizle baslayip baslamadigini kontrol et
    std::string prefix = pendingKeys_ + " ";
    bool hasPrefix = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string current = keymapName;
        while (!current.empty() && !hasPrefix) {
            auto it = keymaps_.find(current);
            if (it == keymaps_.end()) break;
            for (auto& b : it->second.bindings) {
                if (b.keys.size() > pendingKeys_.size() &&
                    b.keys.substr(0, prefix.size()) == prefix) {
                    hasPrefix = true;
                    break;
                }
            }
            current = it->second.parent;
        }
    }

    if (hasPrefix) {
        return ""; // Waiting for more keys / Daha fazla tus bekleniyor
    }

    // No match and no prefix: unbound
    // Esleme yok ve on ek yok: bagli degil
    pendingKeys_.clear();
    return "UNBOUND";
}

// Reset prefix state
// On ek durumunu sifirla
void KeymapManager::resetPrefix() {
    pendingKeys_.clear();
}

// List all bindings in a keymap
// Bir tus haritasindaki tum baglantilari listele
std::vector<KeyBinding> KeymapManager::listBindings(const std::string& keymapName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = keymaps_.find(keymapName);
    if (it != keymaps_.end()) return it->second.bindings;
    return {};
}

// List all keymap names
// Tum tus haritasi adlarini listele
std::vector<std::string> KeymapManager::listKeymaps() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(keymaps_.size());
    for (auto& [name, km] : keymaps_) {
        result.push_back(name);
    }
    return result;
}
