// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "FileWatcher.h"
#include "Logger.h"

namespace fs = std::filesystem;

FileWatcher::FileWatcher() = default;

FileWatcher::~FileWatcher() {
    stop();
}

// Start watching — takes initial snapshot and spawns background thread
// Izlemeye basla — ilk snapshot'i al ve arka plan thread'ini baslat
void FileWatcher::watch(const std::string& dirPath) {
    if (watching_.load()) return;

    watchDir_ = dirPath;
    watching_ = true;

    // Default ignored directories (prevents self-triggering from log writes, etc.)
    // Varsayilan yok sayilan dizinler (log yazimlarindan kaynaklanan kendi kendini tetiklemeyi onler)
    if (ignoreDirs_.empty()) {
        setIgnoreDirs({"logs"});
    }

    // Take initial snapshot so we know the baseline (no events emitted)
    // Ilk snapshot'i al ki temel durumu bilelim (olay yayinlanmaz)
    lastSnapshot_ = takeSnapshot();

    LOG_INFO("[FileWatcher] Watching: ", dirPath,
             " (", lastSnapshot_.size(), " entries, interval=",
             interval_.count(), "ms)");

    watchThread_ = std::thread([this]() { loop(); });
}

// Stop watching and join thread
// Izlemeyi durdur ve thread'i bekle
void FileWatcher::stop() {
    if (!watching_.load()) return;
    watching_ = false;

    if (watchThread_.joinable()) {
        watchThread_.join();
    }

    LOG_INFO("[FileWatcher] Stopped watching: ", watchDir_);
}

// Register event callback (thread-safe)
// Olay callback'i kaydet (thread-guvenli)
void FileWatcher::onEvent(FileEventCallback cb) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callbacks_.push_back(std::move(cb));
}

void FileWatcher::setInterval(std::chrono::milliseconds ms) {
    interval_ = ms;
}

void FileWatcher::setExtensions(const std::vector<std::string>& exts) {
    extensions_ = exts;
}

void FileWatcher::setIgnoreDirs(const std::vector<std::string>& dirs) {
    ignoreDirs_ = dirs;
}

// Background thread loop — snapshot, diff, sleep, repeat
// Arka plan thread dongusu — snapshot, karsilastir, bekle, tekrarla
void FileWatcher::loop() {
    while (watching_.load()) {
        std::this_thread::sleep_for(interval_);

        if (!watching_.load()) break;

        try {
            Snapshot current = takeSnapshot();
            diff(lastSnapshot_, current);
            lastSnapshot_ = std::move(current);
        } catch (const std::exception& e) {
            LOG_ERROR("[FileWatcher] Error: ", e.what());
        }
    }
}

// Take a full recursive snapshot of the directory
// Dizinin tam tekrarlamali snapshot'ini al
FileWatcher::Snapshot FileWatcher::takeSnapshot() {
    Snapshot snap;

    if (!fs::exists(watchDir_)) return snap;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(
                 watchDir_, fs::directory_options::skip_permission_denied)) {

            // Skip ignored directories (e.g. logs/)
            // Yok sayilan dizinleri atla (orn. logs/)
            if (isIgnoredPath(entry.path())) continue;

            bool isDir = entry.is_directory();

            // For files: apply extension filter
            // Dosyalar icin: uzanti filtresini uygula
            if (!isDir && !matchesFilter(entry.path())) continue;

            Entry e;
            e.isDirectory = isDir;

            if (isDir) {
                e.mtime = {};
                e.size = 0;
            } else {
                try {
                    e.mtime = fs::last_write_time(entry);
                    e.size = entry.file_size();
                } catch (...) {
                    // File may have been deleted between iteration and stat
                    // Dosya iteration ile stat arasinda silinmis olabilir
                    continue;
                }
            }

            snap[entry.path().string()] = e;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("[FileWatcher] Snapshot error: ", e.what());
    }

    return snap;
}

// Compare previous and current snapshots, emit events for all differences
// Onceki ve mevcut snapshot'lari karsilastir, tum farkliliklar icin olay yayinla
void FileWatcher::diff(const Snapshot& prev, const Snapshot& curr) {
    // Detect created and modified entries
    // Olusturulan ve degistirilen girisleri tespit et
    for (const auto& [path, entry] : curr) {
        auto it = prev.find(path);
        if (it == prev.end()) {
            // New entry — created
            // Yeni giris — olusturuldu
            emit({FileEvent::Created, path, entry.isDirectory});
        } else if (!entry.isDirectory) {
            // Existing file — check for modification (mtime or size changed)
            // Mevcut dosya — degisiklik kontrol et (mtime veya boyut degisti)
            const auto& old = it->second;
            if (entry.mtime != old.mtime || entry.size != old.size) {
                emit({FileEvent::Modified, path, false});
            }
        }
    }

    // Detect deleted entries
    // Silinen girisleri tespit et
    for (const auto& [path, entry] : prev) {
        if (curr.find(path) == curr.end()) {
            emit({FileEvent::Deleted, path, entry.isDirectory});
        }
    }
}

// Emit event to all registered callbacks
// Tum kayitli callback'lere olay yayinla
void FileWatcher::emit(const FileEventData& event) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    for (const auto& cb : callbacks_) {
        try {
            cb(event);
        } catch (const std::exception& e) {
            LOG_ERROR("[FileWatcher] Callback error: ", e.what());
        }
    }
}

// Check if file extension matches the filter (empty filter = pass all)
// Dosya uzantisinin filtreyle eslesip eslesmedigini kontrol et (bos filtre = hepsini gecir)
bool FileWatcher::matchesFilter(const fs::path& p) const {
    if (extensions_.empty()) return true;
    std::string ext = p.extension().string();
    for (const auto& e : extensions_) {
        if (ext == e) return true;
    }
    return false;
}

// Check if any path segment matches an ignored directory name
// Yol segmentlerinden herhangi birinin yok sayilan dizin ismiyle eslesip eslesmedigini kontrol et
bool FileWatcher::isIgnoredPath(const fs::path& p) const {
    if (ignoreDirs_.empty()) return false;
    for (const auto& segment : p) {
        const std::string name = segment.filename().string();
        for (const auto& dir : ignoreDirs_) {
            if (name == dir) return true;
        }
    }
    return false;
}
