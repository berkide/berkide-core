// ==========================
// 🧠 BerkIDE Runtime Core
// ==========================

// Editor çekirdek API
globalThis.editor = globalThis.editor || {};
editor.version = "0.1.0-dev";
editor.state = {};

// --------------------------
// 📜 Command sistemini başlat
// --------------------------
editor.commands = editor.commands || {};
editor.commands.map = new Map();

// Native tarafla köprü (__nativeExec)
editor.commands.exec = function (name, args = {}) {
  if (typeof name !== "string") {
    console.error("[Core JS] Invalid command name:", name);
    return;
  }

  if (editor.commands.map.has(name)) {
       // JS tarafında tanımlıysa doğrudan çağır
    try {
      return editor.commands.map.get(name)(args);
    } catch (e) {
      console.error("[Core JS] JS command error:", e);
    }
  } else {
    // değilse native’e gönder
      try {
    const result = editor.commands.__nativeExec(name, JSON.stringify(args));
    return JSON.parse(result || "{}");
  } catch (e) {
    console.error("[Core JS] Command exec failed:", e);
  }
  }



};

// Register JS-level commands
editor.commands.register = function (name, fn) {
  if (typeof fn !== "function") {
    console.error("[Core] register() expects function, got:", typeof fn);
    return;
  }
  editor.commands.map.set(name, fn);
  console.log("[Core] JS command registered:", name);
};

// Execute JS-level command (or native fallback)
editor.commands.run = async function (name, args = {}) {
  if (editor.commands.map.has(name)) {
    try {
      return await editor.commands.map.get(name)(args);
    } catch (e) {
      console.error("[Core] Command error in:", name, e);
    }
  } else {
    return editor.commands.exec(name, args);
  }
};

// --------------------------
// 🎧 Event sistemi
// --------------------------
// editor.events = {
//   _listeners: {},

//   on(event, fn) {
//     if (!this._listeners[event]) this._listeners[event] = [];
//     this._listeners[event].push(fn);
//   },

//   emit(event, data) {
//     const listeners = this._listeners[event] || [];
//     for (const fn of listeners) {
//       try {
//         fn(data);
//       } catch (e) {
//         console.error(`[Core] Event '${event}' error:`, e);
//       }
//     }
//   },
// };

// --------------------------
// 🗝️ Keymap sistemi (boş)
// --------------------------
editor.keymap = {
  bindings: {},
  bind(key, command) {
    this.bindings[key] = command;
  },
  trigger(key) {
    const cmd = this.bindings[key];
    if (cmd) editor.commands.run(cmd);
  },
};


// --------------------------
// 🔧 http server 
// --------------------------

console.log("[HTTP] server initializing...");
const PORT = 1881
editor.http.listen(PORT);

console.log(`[HTTP] ready at http://localhost:${PORT}`);


// --------------------------
// 🔧 socket server 
// --------------------------

console.log("[WS] server initializing...");
const WSPORT = 1882
editor.ws.listen(WSPORT);

console.log(`[WS] ready at ws://localhost:${WSPORT}`);


editor.events.on("testEvent", (data) => {
  console.log("[JS] testEvent geldi:", data);
});


editor.keymap.bind("Ctrl+S", "file.saveAs");

// --------------------------
// 🔧 Başlangıç mesajı
// --------------------------
console.log(`[BerkIDE Core] Runtime loaded. v${editor.version}`);