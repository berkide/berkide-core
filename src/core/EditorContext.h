// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once

// Forward declarations for all core editor components
// Tum temel editor bilesenlerinin on bildirimleri
class Buffers;
class InputHandler;
class EventBus;
class FileSystem;
class HttpServer;
class WebSocketServer;
class PluginManager;
class HelpSystem;
class ProcessManager;
class RegisterManager;
class SearchEngine;
class MarkManager;
class AutoSave;
class ExtmarkManager;
class MacroRecorder;
class KeymapManager;
class FoldManager;
class CommandRouter;
class DiffEngine;
class CompletionEngine;
class MultiCursor;
class WindowManager;
class TreeSitterEngine;
class SessionManager;
class EncodingDetector;
class CharClassifier;
class IndentEngine;
class WorkerManager;
class BufferOptions;
class I18n;

// Central context struct that holds pointers to all real editor objects.
// Tum gercek editor nesnelerine isaret eden merkezi baglam yapisi.
// Passed to V8 bindings so JavaScript operates on the same instances as main.cpp.
// V8 binding'lerine aktarilir, boylece JavaScript main.cpp ile ayni nesneler uzerinde calisir.
struct EditorContext {
    Buffers*          buffers       = nullptr;  // Multi-buffer manager / Coklu buffer yoneticisi
    InputHandler*     input         = nullptr;  // Keyboard input handler / Klavye girdi isleyicisi
    EventBus*         eventBus      = nullptr;  // Pub/sub event system / Yayinla/abone ol olay sistemi
    FileSystem*       fileSystem    = nullptr;  // File I/O operations / Dosya giris/cikis islemleri
    HttpServer*       httpServer    = nullptr;  // REST API server / REST API sunucusu
    WebSocketServer*  wsServer      = nullptr;  // WebSocket server / WebSocket sunucusu
    PluginManager*    pluginManager = nullptr;  // Plugin lifecycle manager / Eklenti yasam dongusu yoneticisi
    HelpSystem*       helpSystem    = nullptr;  // Offline help/wiki system / Cevrimdisi yardim/wiki sistemi
    ProcessManager*   processManager = nullptr; // Subprocess lifecycle manager / Alt surec yasam dongusu yoneticisi
    RegisterManager*  registers      = nullptr; // Named register/clipboard system / Adlandirilmis register/pano sistemi
    SearchEngine*     searchEngine   = nullptr; // Find/replace engine / Bul/degistir motoru
    MarkManager*      markManager    = nullptr; // Named marks and jump list / Adlandirilmis isaretler ve atlama listesi
    AutoSave*         autoSave       = nullptr; // Auto-save and backup system / Otomatik kaydetme ve yedekleme sistemi
    ExtmarkManager*   extmarkManager = nullptr; // Text decorations/properties / Metin dekorasyonlari/ozellikleri
    MacroRecorder*    macroRecorder  = nullptr; // Command recording/playback / Komut kayit/oynatma
    KeymapManager*    keymapManager  = nullptr; // Hierarchical key bindings / Hiyerarsik tus baglantilari
    FoldManager*      foldManager    = nullptr; // Code folding regions / Kod katlama bolgeleri
    CommandRouter*    commandRouter  = nullptr; // Command dispatch for macro playback / Makro oynatma icin komut dagitici
    DiffEngine*       diffEngine     = nullptr; // Line-based diff (Myers algorithm) / Satir bazli diff (Myers algoritmasi)
    CompletionEngine* completionEngine = nullptr; // Fuzzy completion scoring / Bulanik tamamlama puanlama
    MultiCursor*      multiCursor    = nullptr; // Multiple cursor editing / Coklu imlec duzenleme
    WindowManager*    windowManager  = nullptr; // Split window layout / Bolunmus pencere duzeni
    TreeSitterEngine* treeSitter     = nullptr; // Syntax parsing engine / Soz dizimi ayristirma motoru
    SessionManager*   sessionManager = nullptr; // Session persistence / Oturum kaliciligi
    EncodingDetector* encodingDetector = nullptr; // Encoding detection/conversion / Kodlama algilama/donusturme
    CharClassifier*   charClassifier   = nullptr; // Character classification and word boundaries / Karakter siniflandirma ve kelime sinirlari
    IndentEngine*     indentEngine     = nullptr; // Auto-indent engine / Otomatik girinti motoru
    WorkerManager*    workerManager    = nullptr; // Background V8 worker threads / Arka plan V8 calisan thread'leri
    BufferOptions*    bufferOptions    = nullptr; // Per-buffer and global options / Buffer-bazli ve global secenekler
    I18n*             i18n             = nullptr; // Internationalization system / Uluslararasilastirma sistemi
};
