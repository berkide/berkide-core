// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

// ==========================
// BerkIDE Event Runtime
// BerkIDE Olay Sistemi
// ==========================

console.log("[Events] Initializing...");

editor.__sources.js.events = true;

// Event bus — publish/subscribe pattern for editor events
// Olay veri yolu — editor olaylari icin yayinla/abone ol deseni
editor.events = editor.events || {
  _listeners: {},

  // Subscribe to an event
  // Bir olaya abone ol
  on(event, fn) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(fn);
  },

  // Subscribe to an event, auto-remove after first call
  // Bir olaya abone ol, ilk cagirdan sonra otomatik kaldir
  once(event, fn) {
    const wrapper = (data) => {
      this.off(event, wrapper);
      fn(data);
    };
    wrapper._original = fn;
    this.on(event, wrapper);
  },

  // Unsubscribe from an event
  // Bir olaydan aboneligi kaldir
  off(event, fn) {
    if (!this._listeners[event]) return;
    this._listeners[event] = this._listeners[event].filter(
      (f) => f !== fn && f._original !== fn
    );
  },

  // Emit an event to all subscribers
  // Tum abonelere olay yayinla
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

  // Remove all listeners for an event (or all events if no name given)
  // Bir olayin tum dinleyicilerini kaldir (isim verilmezse tum olaylar)
  removeAll(event) {
    if (event) {
      delete this._listeners[event];
    } else {
      this._listeners = {};
    }
  },
};

// --------------------------
// Built-in event listeners
// Yerlesik olay dinleyicileri
// --------------------------

// File loaded
// Dosya yuklendi
editor.events.on("fileLoaded", (info) => {
  console.log(`[Events] File loaded: ${info?.path || "unknown"}`);
});

// File saved
// Dosya kaydedildi
editor.events.on("fileSaved", (info) => {
  console.log(`[Events] File saved: ${info?.path || "unknown"}`);
});

// Command executed
// Komut yurutuldu
editor.events.on("commandExecuted", (info) => {
  console.log(`[Events] Command executed: ${info?.name}`);
});

// Buffer changed
// Buffer degisti
editor.events.on("bufferChanged", (path) => {
  console.log(`[Events] Buffer changed: ${path || "unknown"}`);
});

console.log("[Events] Runtime loaded.");
