// ==========================
// 🔔 BerkIDE Event Runtime
// ==========================

console.log("[Events] initializing...");

editor.events = editor.events || {
  _listeners: {},
  on(event, fn) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(fn);
  },
  emit(event, data) {
    const listeners = this._listeners[event] || [];
    for (const fn of listeners) {
      try {
        fn(data);
      } catch (e) {
        console.error(`[Events] Error in '${event}':`, e);
      }
    }
  },
};

// --------------------------
// 📡 Sistem olayları
// --------------------------

// Dosya yüklendiğinde
editor.events.on("fileLoaded", (info) => {
  console.log(`[Events] File loaded: ${info?.path || "unknown"}`);
});

// Dosya kaydedildiğinde
editor.events.on("fileSaved", (info) => {
  console.log(`[Events] File saved: ${info?.path || "unknown"}`);
});

// Komut yürütüldüğünde
editor.events.on("commandExecuted", (info) => {
  console.log(`[Events] Command executed: ${info?.name}`);
});

// Örnek tetikleme (ileride C++ tarafı da tetikleyebilir)
setTimeout(() => {
  editor.events.emit("fileLoaded", { path: "test.txt" });
  editor.events.emit("commandExecuted", { name: "buffer.save" });
}, 1000);

console.log("[Events] Runtime loaded.");