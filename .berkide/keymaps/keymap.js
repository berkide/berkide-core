// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

// ==========================
// BerkIDE Keymap Runtime
// BerkIDE Tus Haritasi Calisma Zamani
// ==========================

console.log("[Keymap] Initializing...");

// Track JS source provenance
// JS kaynak izlemesini kaydet
editor.__sources.js.keymap = true;

// Global keymap system — loads bindings from JSON, binds keys to commands
// Global tus haritasi sistemi — JSON'dan binding yukler, tuslari komutlara baglar
editor.keymap = {
  bindings: {},
  sourceFile: "default.json",

  // Load keybindings from a JSON file
  // Bir JSON dosyasindan tus baglamalarini yukle
  async load(path = this.sourceFile) {
    try {
      const response = editor.file.loadText(path);
      if (!response || !response.ok) {
        throw new Error(response?.message || "load failed");
      }
      const json = JSON.parse(response.data);
      this.bindings = json;
      console.log(`[Keymap] Loaded ${Object.keys(json).length} bindings from ${path}`);
      this.applyAll();
    } catch (err) {
      console.error(`[Keymap] Failed to load ${path}:`, err);
    }
  },

  // Save current keybindings to a JSON file
  // Mevcut tus baglamalarini bir JSON dosyasina kaydet
  async save(path = this.sourceFile) {
    try {
      const jsonStr = JSON.stringify(this.bindings, null, 2);
      const result = editor.file.saveText(path, jsonStr);
      if (!result || !result.ok) {
        throw new Error(result?.message || "save failed");
      }
      console.log(`[Keymap] Saved bindings to ${path}`);
    } catch (err) {
      console.error(`[Keymap] Failed to save ${path}:`, err);
    }
  },

  // Bind a key chord to a command name
  // Bir tus kombinasyonunu komut ismine bagla
  bind(key, cmdName) {
    this.bindings[key] = cmdName;
    editor.input.bindChord(key, () => editor.commands.run(cmdName));
    console.log(`[Keymap] Bound '${key}' -> ${cmdName}`);
  },

  // Remove a key binding
  // Bir tus baglamasini kaldir
  unbind(key) {
    delete this.bindings[key];
    console.log(`[Keymap] Unbound '${key}'`);
  },

  // Apply all loaded bindings to the input system
  // Tum yuklenen baglmalari input sistemine uygula
  applyAll() {
    for (const [key, cmd] of Object.entries(this.bindings)) {
      editor.input.bindChord(key, () => editor.commands.run(cmd));
    }
    console.log(`[Keymap] Applied ${Object.keys(this.bindings).length} bindings.`);
  },

  // Trigger the command bound to a key chord
  // Bir tus kombinasyonuna bagli komutu tetikle
  trigger(key) {
    const cmd = this.bindings[key];
    if (!cmd) {
      console.warn(`[Keymap] No command bound for '${key}'`);
      return;
    }
    editor.commands.run(cmd);
  }
};

// Listen for keyDown events from the native input system
// Native input sisteminden keyDown olaylarini dinle
editor.input.registerOnKeyDown((ev) => {
  if (ev && ev.chord) {
    editor.keymap.trigger(ev.chord);
  }
});

// Load default keymap on startup
// Baslangiçta varsayilan tus haritasini yukle
(async () => {
  await editor.keymap.load("default.json");
  console.log("[Keymap] Runtime ready.");
})();
