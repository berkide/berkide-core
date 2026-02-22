// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

// ==========================
// BerkIDE Light Theme
// BerkIDE Acik Tema
// ==========================

editor.themes = editor.themes || {};

// Light theme definition — based on modern editor conventions
// Acik tema tanimi — modern editor konvansiyonlarina dayali
editor.themes.light = {
  name: "BerkIDE Light",
  type: "light",

  // Editor core colors
  // Editor cekirdek renkleri
  editor: {
    background: "#eff1f5",
    foreground: "#4c4f69",
    lineNumber: "#9ca0b0",
    lineNumberActive: "#4c4f69",
    cursor: "#dc8a78",
    selection: "#ccd0da",
    selectionMatch: "#bcc0cc",
    currentLine: "#e6e9ef",
    indentGuide: "#ccd0da",
    whitespace: "#bcc0cc",
    matchBracket: "#ea76cb",
  },

  // Syntax highlighting tokens
  // Sozdizimi vurgulama tokenlari
  syntax: {
    keyword: "#8839ef",
    string: "#40a02b",
    number: "#fe640b",
    comment: "#9ca0b0",
    function: "#1e66f5",
    variable: "#4c4f69",
    type: "#df8e1d",
    constant: "#fe640b",
    operator: "#04a5e5",
    punctuation: "#6c6f85",
    property: "#1e66f5",
    tag: "#8839ef",
    attribute: "#df8e1d",
    regexp: "#d20f39",
    error: "#d20f39",
    link: "#1e66f5",
  },

  // UI chrome colors
  // Arayuz krom renkleri
  ui: {
    tabActive: "#eff1f5",
    tabInactive: "#e6e9ef",
    tabBorder: "#ccd0da",
    statusBar: "#e6e9ef",
    statusBarText: "#5c5f77",
    sideBar: "#e6e9ef",
    sideBarText: "#5c5f77",
    panel: "#e6e9ef",
    panelBorder: "#ccd0da",
    scrollbar: "#bcc0cc",
    scrollbarHover: "#acb0be",
    tooltip: "#ccd0da",
    tooltipText: "#4c4f69",
    menuBackground: "#eff1f5",
    menuForeground: "#4c4f69",
    menuSelection: "#ccd0da",
  },

  // Diff / git colors
  // Diff / git renkleri
  diff: {
    added: "#40a02b",
    removed: "#d20f39",
    modified: "#df8e1d",
    conflict: "#fe640b",
  },

  // Diagnostic colors
  // Tani renkleri
  diagnostic: {
    error: "#d20f39",
    warning: "#fe640b",
    info: "#1e66f5",
    hint: "#40a02b",
  },
};

console.log(`[Theme] '${editor.themes.light.name}' loaded.`);
