// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include "V8Engine.h"
#include <atomic>

// Global restart flag — set by file watchers, checked by main loop
// Global yeniden baslatma bayragi — dosya izleyicileri tarafindan ayarlanir, ana dongu tarafindan kontrol edilir
extern std::atomic<bool> g_restartRequested;

// Initialize the V8 engine (platform, isolate, context, bindings)
// V8 motorunu baslat (platform, izolasyon, baglam, binding'ler)
void StartEngine(V8Engine& eng);

// Create .berkide/ directory structure and log paths
// .berkide/ dizin yapisini olustur ve yollari logla
void CreateInitBerkideAndLoad(V8Engine& eng);

// Load JavaScript files from .berkide/ (runtime, keymaps, events, plugins)
// .berkide/ dizininden JavaScript dosyalarini yukle (runtime, keymaps, events, plugins)
void LoadBerkideEnvironment(V8Engine& eng);

// Start file watchers on .berkide directories (sets g_restartRequested on changes)
// .berkide dizinlerinde dosya izleyicileri baslat (degisikliklerde g_restartRequested set eder)
void StartWatchers();

// Stop all file watchers
// Tum dosya izleyicilerini durdur
void StopWatchers();

// Run the main editor loop (legacy, used in non-headless mode)
// Ana editor dongusunu calistir (eski, basliksiz olmayan modda kullanilir)
void StartEditorLoop(V8Engine& eng);

// Gracefully shutdown the V8 engine
// V8 motorunu duzgun kapat
void ShutdownEngine(V8Engine& eng);
