#pragma once
#include "V8Engine.h"

// Initialize the V8 engine (platform, isolate, context, bindings)
// V8 motorunu baslat (platform, izolasyon, baglam, binding'ler)
void StartEngine(V8Engine& eng);

// Create .berkide/ directory structure and log paths
// .berkide/ dizin yapisini olustur ve yollari logla
void CreateInitBerkideAndLoad(V8Engine& eng);

// Load JavaScript files from .berkide/ (runtime, keymaps, events, plugins)
// .berkide/ dizininden JavaScript dosyalarini yukle (runtime, keymaps, events, plugins)
void LoadBerkideEnvironment(V8Engine& eng);

// Start the file watcher for plugin hot-reload
// Eklenti anlik yeniden yuklemesi icin dosya izleyicisini baslat
void StartPluginWatcher(V8Engine& eng);

// Run the main editor loop (legacy, used in non-headless mode)
// Ana editor dongusunu calistir (eski, basliksiz olmayan modda kullanilir)
void StartEditorLoop(V8Engine& eng);

// Gracefully shutdown the V8 engine and all watchers
// V8 motorunu ve tum izleyicileri duzgun kapat
void ShutdownEngine(V8Engine& eng);
