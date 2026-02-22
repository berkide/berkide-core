// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

// ==========================
// BerkIDE Dark Theme
// BerkIDE Karanlik Tema
// ==========================

editor.themes = editor.themes || {};

// Dark theme definition — based on modern editor conventions
// Karanlik tema tanimi — modern editor konvansiyonlarina dayali
editor.themes.dark = {
  name: "BerkIDE Dark",
  type: "dark",

  // Editor core colors
  // Editor cekirdek renkleri
  editor: {
    background: "#1e1e2e",
    foreground: "#cdd6f4",
    lineNumber: "#6c7086",
    lineNumberActive: "#cdd6f4",
    cursor: "#f5e0dc",
    selection: "#45475a",
    selectionMatch: "#585b70",
    currentLine: "#313244",
    indentGuide: "#313244",
    whitespace: "#45475a",
    matchBracket: "#f5c2e7",
  },

  // Syntax highlighting tokens
  // Sozdizimi vurgulama tokenlari
  syntax: {
    keyword: "#cba6f7",
    string: "#a6e3a1",
    number: "#fab387",
    comment: "#6c7086",
    function: "#89b4fa",
    variable: "#cdd6f4",
    type: "#f9e2af",
    constant: "#fab387",
    operator: "#89dceb",
    punctuation: "#9399b2",
    property: "#89b4fa",
    tag: "#cba6f7",
    attribute: "#f9e2af",
    regexp: "#f38ba8",
    error: "#f38ba8",
    link: "#89b4fa",
  },

  // UI chrome colors
  // Arayuz krom renkleri
  ui: {
    tabActive: "#1e1e2e",
    tabInactive: "#181825",
    tabBorder: "#313244",
    statusBar: "#181825",
    statusBarText: "#bac2de",
    sideBar: "#181825",
    sideBarText: "#bac2de",
    panel: "#181825",
    panelBorder: "#313244",
    scrollbar: "#45475a",
    scrollbarHover: "#585b70",
    tooltip: "#313244",
    tooltipText: "#cdd6f4",
    menuBackground: "#1e1e2e",
    menuForeground: "#cdd6f4",
    menuSelection: "#45475a",
  },

  // Diff / git colors
  // Diff / git renkleri
  diff: {
    added: "#a6e3a1",
    removed: "#f38ba8",
    modified: "#f9e2af",
    conflict: "#fab387",
  },

  // Diagnostic colors
  // Tani renkleri
  diagnostic: {
    error: "#f38ba8",
    warning: "#fab387",
    info: "#89b4fa",
    hint: "#a6e3a1",
  },
};

// Set dark as the active theme
// Karanlik temayi aktif tema olarak ayarla
editor.theme = editor.themes.dark;

console.log(`[Theme] '${editor.theme.name}' loaded.`);
