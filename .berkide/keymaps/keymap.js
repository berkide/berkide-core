// ==========================
// ⌨️ BerkIDE Keymap Runtime
// ==========================
console.log("[Keymap] Initializing...");

// Global keymap sistemi
editor.keymap = {
  bindings: {},
  sourceFile: "keymaps/default.json",

  // --- keymap.load(path) ---
  async load(path = this.sourceFile) {
    try {
      const response = await editor.file.loadText(path);
      const json = JSON.parse(response);
      this.bindings = json;
      console.log(`[Keymap] Loaded ${Object.keys(json).length} bindings from ${path}`);
      this.applyAll();
    } catch (err) {
      console.error(`[Keymap] Failed to load ${path}:`, err);
    }
  },

  // --- keymap.save(path) ---
  async save(path = this.sourceFile) {
    try {
      const jsonStr = JSON.stringify(this.bindings, null, 2);
      await editor.file.saveText(path, jsonStr);
      console.log(`[Keymap] Saved bindings to ${path}`);
    } catch (err) {
      console.error(`[Keymap] Failed to save ${path}:`, err);
    }
  },

  // --- keymap.bind(key, cmdName) ---
  bind(key, cmdName) {
    this.bindings[key] = cmdName;
    editor.input.bindChord(key, () => editor.commands.run(cmdName));
    console.log(`[Keymap] Bound '${key}' -> ${cmdName}`);
  },

  // --- keymap.unbind(key) ---
  unbind(key) {
    delete this.bindings[key];
    console.log(`[Keymap] Unbound '${key}'`);
  },

  // --- keymap.applyAll() ---
  applyAll() {
    for (const [key, cmd] of Object.entries(this.bindings)) {
      editor.input.bindChord(key, () => editor.commands.run(cmd));
    }
    console.log(`[Keymap] Applied ${Object.keys(this.bindings).length} bindings.`);
  },

  // --- keymap.trigger(key) ---
  trigger(key) {
    const cmd = this.bindings[key];
    if (!cmd) {
      console.warn(`[Keymap] No command bound for '${key}'`);
      return;
    }
    console.log(`[Keymap] Triggering ${cmd} (${key})`);
    editor.commands.run(cmd);
  }
};

// Olay dinleyicisi — input’tan gelen keyDown eventlerini yakalar
editor.input.registerOnKeyDown((ev) => {
  if (ev && ev.chord) {
    editor.keymap.trigger(ev.chord);
  }
});

// Başlangıçta varsayılan keymap’i yükle
(async () => {
  await editor.keymap.load("keymaps/default.json");
  console.log("[Keymap] Runtime ready.");
})();