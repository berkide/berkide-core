// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>

// File system event types for directory watching
// Dizin izleme icin dosya sistemi olay turleri
enum class FileEvent {
    Created,       // New file or directory appeared / Yeni dosya veya dizin belirdi
    Modified,      // File content changed (mtime or size) / Dosya icerigi degisti (mtime veya boyut)
    Deleted        // File or directory removed / Dosya veya dizin silindi
};

// Data associated with a file system event
// Dosya sistemi olayiyla iliskili veri
struct FileEventData {
    FileEvent   type;
    std::string path;
    bool        isDirectory;
};

// Callback signature for file system events
// Dosya sistemi olaylari icin callback imzasi
using FileEventCallback = std::function<void(const FileEventData&)>;

// Generic recursive directory watcher using polling-based snapshot comparison.
// Yoklama tabanli snapshot karsilastirmasi kullanan genel tekrarlamali dizin izleyicisi.
// Detects: file/directory creation, modification (mtime+size), deletion.
// Tespit eder: dosya/dizin olusturma, degistirme (mtime+boyut), silme.
// Thread-safe: callbacks are invoked from the watcher thread.
// Thread-guvenli: callback'ler izleyici thread'inden cagrilir.
class FileWatcher {
public:
    FileWatcher();
    ~FileWatcher();

    // Start watching a directory recursively (non-blocking, spawns background thread)
    // Bir dizini tekrarlamali olarak izlemeye basla (engellemez, arka plan thread'i baslatir)
    void watch(const std::string& dirPath);

    // Stop watching and join the background thread
    // Izlemeyi durdur ve arka plan thread'ini bekle
    void stop();

    // Register a callback for file system events
    // Dosya sistemi olaylari icin callback kaydet
    void onEvent(FileEventCallback cb);

    // Set polling interval (default 1000ms)
    // Yoklama araligini ayarla (varsayilan 1000ms)
    void setInterval(std::chrono::milliseconds ms);

    // Set file extension filter (empty = all files). Example: {".js", ".mjs"}
    // Dosya uzantisi filtresi ayarla (bos = tum dosyalar). Ornek: {".js", ".mjs"}
    void setExtensions(const std::vector<std::string>& exts);

    // Set directory names to ignore during watching. Example: {"logs", "cache"}
    // Izleme sirasinda yok sayilacak dizin isimlerini ayarla. Ornek: {"logs", "cache"}
    void setIgnoreDirs(const std::vector<std::string>& dirs);

    // Check if currently watching
    // Su an izleme yapilip yapilmadigini kontrol et
    bool isWatching() const { return watching_.load(); }

    // Get the watched directory path
    // Izlenen dizin yolunu al
    const std::string& watchPath() const { return watchDir_; }

private:
    // Snapshot entry: file metadata for comparison
    // Snapshot girdisi: karsilastirma icin dosya metaverisi
    struct Entry {
        std::filesystem::file_time_type mtime;
        std::uintmax_t                  size;
        bool                            isDirectory;
    };

    using Snapshot = std::unordered_map<std::string, Entry>;

    // Take a full snapshot of the watched directory tree
    // Izlenen dizin agacinin tam snapshot'ini al
    Snapshot takeSnapshot();

    // Compare two snapshots and emit events for differences
    // Iki snapshot'i karsilastir ve farkliliklar icin olay yayinla
    void diff(const Snapshot& prev, const Snapshot& curr);

    // Emit an event to all registered callbacks
    // Tum kayitli callback'lere olay yayinla
    void emit(const FileEventData& event);

    // Check if a path passes the extension filter
    // Bir yolun uzanti filtresinden gecip gecmedigini kontrol et
    bool matchesFilter(const std::filesystem::path& p) const;

    // Check if a path falls under an ignored directory
    // Bir yolun yok sayilan bir dizin altinda olup olmadigini kontrol et
    bool isIgnoredPath(const std::filesystem::path& p) const;

    // Background thread main loop
    // Arka plan thread'i ana dongusu
    void loop();

    std::string                     watchDir_;
    std::atomic<bool>               watching_{false};
    std::thread                     watchThread_;
    std::chrono::milliseconds       interval_{1000};
    std::vector<std::string>        extensions_;
    std::vector<std::string>        ignoreDirs_;
    std::vector<FileEventCallback>  callbacks_;
    std::mutex                      callbackMutex_;
    Snapshot                        lastSnapshot_;
};
