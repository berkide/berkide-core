// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

// ==========================
// BerkIDE Runtime Core
// BerkIDE Calisma Zamani Cekirdegi
// ==========================

// Initialize editor core API
// Editor cekirdek API'sini baslat
globalThis.editor = globalThis.editor || {};
editor.version = "0.1.0-dev";
editor.state = {};
editor.__sources.js.state = true;

// --------------------------
// Command System
// Komut Sistemi
// --------------------------

// Command map: stores JS-level commands
// Komut haritasi: JS seviyesindeki komutlari tutar
editor.commands = editor.commands || {};
editor.commands.map = new Map();

// Execute a command — checks JS map first, then falls through to native (C++)
// Komut calistir — once JS haritasini kontrol eder, yoksa native (C++) tarafina gonderir
editor.commands.exec = function (name, args = {}) {
  if (typeof name !== "string") {
    console.error("[Core] Invalid command name:", name);
    return;
  }

  if (editor.commands.map.has(name)) {
    try {
      return editor.commands.map.get(name)(args);
    } catch (e) {
      console.error("[Core] JS command error:", e);
    }
  } else {
    try {
      const result = editor.commands.__nativeExec(name, JSON.stringify(args));
      return JSON.parse(result || "{}");
    } catch (e) {
      console.error("[Core] Native command exec failed:", e);
    }
  }
};

// Register a JS-level command by name
// JS seviyesinde isimle komut kaydet
editor.commands.register = function (name, fn) {
  if (typeof fn !== "function") {
    console.error("[Core] register() expects function, got:", typeof fn);
    return;
  }
  editor.commands.map.set(name, fn);
  console.log("[Core] JS command registered:", name);
};

// Run a command (async-safe) — JS first, native fallback
// Komut calistir (async-guvenli) — once JS, yoksa native
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
// Startup
// Baslangic
// --------------------------
console.log(`[Core] Runtime loaded. v${editor.version}`);
