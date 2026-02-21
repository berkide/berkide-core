#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>

// A single key binding: maps a key sequence to a command
// Tek bir tus baglantisi: bir tus dizisini bir komuta esler
struct KeyBinding {
    std::string keys;            // Key sequence (e.g., "C-x C-f", "g d") / Tus dizisi
    std::string command;         // Command to execute / Calistirilacak komut
    std::string argsJson;        // Optional command arguments as JSON / Istege bagli JSON komut argumanlari
};

// A keymap: ordered list of key bindings for a specific context
// Bir tus haritasi: belirli bir baglam icin siralanmis tus baglantilari listesi
struct Keymap {
    std::string name;            // Keymap name (e.g., "global", "normal", "insert") / Tus haritasi adi
    std::string parent;          // Parent keymap name for fallback lookup / Geri donus aramaai icin ust tus haritasi
    std::vector<KeyBinding> bindings;
};

// Manages hierarchical keymaps with mode-specific and buffer-local bindings.
// Moda ozel ve buffer-yerel baglamalarla hiyerarsik tus haritalarini yonetir.
// Lookup order: buffer-local -> mode-specific -> global (like Emacs keymap chain).
// Arama sirasi: buffer-yerel -> moda ozel -> global (Emacs tus haritasi zinciri gibi).
// Supports multi-key sequences (C-x C-f) via prefix state machine.
// On ek durum makinesi araciligiyla coklu tus dizilerini (C-x C-f) destekler.
class KeymapManager {
public:
    KeymapManager();

    // Create or get a keymap by name
    // Isme gore bir tus haritasi olustur veya al
    void createKeymap(const std::string& name, const std::string& parent = "");

    // Set a key binding in a specific keymap
    // Belirli bir tus haritasinda tus baglantisi ayarla
    void set(const std::string& keymapName, const std::string& keys,
             const std::string& command, const std::string& argsJson = "");

    // Remove a key binding from a keymap
    // Bir tus haritasindan tus baglantisini kaldir
    bool remove(const std::string& keymapName, const std::string& keys);

    // Lookup a key sequence in the keymap hierarchy
    // Tus haritasi hiyerarsisinde bir tus dizisini ara
    // Returns the matching binding, searching up the parent chain.
    // Ust zincirde arayarak eslesen baglantlyi dondurur.
    const KeyBinding* lookup(const std::string& keymapName, const std::string& keys) const;

    // Feed a key press into the prefix state machine
    // On ek durum makinesine bir tus basisi besle
    // Returns: command name if complete match, "" if prefix (waiting for more keys),
    //          or "UNBOUND" if no binding exists.
    // Dondurur: tam eslesmeyse komut adi, on ekse "" (daha fazla tus bekleniyor),
    //           baglanti yoksa "UNBOUND".
    std::string feedKey(const std::string& keymapName, const std::string& key);

    // Reset the prefix state (cancel multi-key sequence)
    // On ek durumunu sifirla (coklu tus dizisini iptal et)
    void resetPrefix();

    // Get current prefix keys (for status display)
    // Mevcut on ek tuslarini al (durum gosterimi icin)
    const std::string& currentPrefix() const { return pendingKeys_; }

    // Check if we're in the middle of a multi-key sequence
    // Coklu tus dizisinin ortasinda olup olmadigimizi kontrol et
    bool hasPendingPrefix() const { return !pendingKeys_.empty(); }

    // List all bindings in a keymap
    // Bir tus haritasindaki tum baglantilari listele
    std::vector<KeyBinding> listBindings(const std::string& keymapName) const;

    // List all keymap names
    // Tum tus haritasi adlarini listele
    std::vector<std::string> listKeymaps() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Keymap> keymaps_;
    std::string pendingKeys_;    // Accumulated prefix keys / Biriken on ek tuslari
};
