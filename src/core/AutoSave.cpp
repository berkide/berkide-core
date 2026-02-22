// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "AutoSave.h"
#include "buffers.h"
#include "EventBus.h"
#include "Logger.h"
#include <fstream>
#include <chrono>

namespace fs = std::filesystem;

// Default constructor
// Varsayilan kurucu
AutoSave::AutoSave() = default;

// Destructor: stop auto-save thread
// Yikici: otomatik kaydetme thread'ini durdur
AutoSave::~AutoSave() {
    stop();
}

// Start the auto-save background thread
// Otomatik kaydetme arka plan thread'ini baslat
void AutoSave::start() {
    if (running_ || intervalSec_ <= 0) return;

    // Ensure save directory exists
    // Kaydetme dizininin var oldugundan emin ol
    if (!saveDir_.empty()) {
        std::error_code ec;
        fs::create_directories(saveDir_, ec);
    }

    running_ = true;
    saveThread_ = std::thread(&AutoSave::autoSaveLoop, this);
    LOG_INFO("[AutoSave] Started (interval=", intervalSec_, "s, dir=", saveDir_, ")");
}

// Stop the auto-save background thread
// Otomatik kaydetme arka plan thread'ini durdur
void AutoSave::stop() {
    running_ = false;
    if (saveThread_.joinable()) {
        saveThread_.join();
    }
}

// Background auto-save loop
// Arka plan otomatik kaydetme dongusu
void AutoSave::autoSaveLoop() {
    while (running_) {
        // Sleep in small intervals so we can check running_ flag
        // running_ bayragini kontrol edebilmemiz icin kucuk araliklarla uyu
        for (int i = 0; i < intervalSec_ * 10 && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!running_ || !buffers_) continue;

        // Save all modified buffers (read-only access, does NOT change active index)
        // Tum degistirilmis buffer'lari kaydet (salt okunur erisim, aktif indeksi DEGISTIRMEZ)
        size_t count = buffers_->count();
        for (size_t i = 0; i < count; ++i) {
            // Access each buffer's state without mutating active index
            // Aktif indeksi degistirmeden her buffer'in durumuna eris
            const auto& st = buffers_->getStateAt(i);

            if (!st.isModified()) continue;

            const std::string& fp = st.getFilePath();
            if (fp.empty()) continue;

            // Build content string from buffer lines
            // Buffer satirlarindan icerik dizesi olustur
            const auto& buf = st.getBuffer();
            std::string content;
            for (int line = 0; line < buf.lineCount(); ++line) {
                if (line > 0) content += '\n';
                content += buf.getLine(line);
            }

            if (saveBuffer(fp, content)) {
                if (eventBus_) {
                    eventBus_->emit("autoSaved", fp);
                }
            }
        }
    }
}

// Generate recovery file path from original path
// Orijinal yoldan kurtarma dosyasi yolu olustur
std::string AutoSave::recoveryPath(const std::string& filePath) const {
    // Convert /path/to/file.txt -> autosave-dir/path_to_file.txt
    // /yol/dosya.txt -> autosave-dir/yol_dosya.txt'ye donustur
    std::string name = filePath;
    for (char& c : name) {
        if (c == '/' || c == '\\') c = '_';
    }
    return saveDir_ + "/" + name;
}

// Save buffer content to auto-save directory
// Buffer icerigini otomatik kaydetme dizinine kaydet
bool AutoSave::saveBuffer(const std::string& filePath, const std::string& content) {
    if (saveDir_.empty()) return false;

    std::string path = recoveryPath(filePath);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        LOG_WARN("[AutoSave] Failed to write: ", path);
        return false;
    }
    out.write(content.c_str(), static_cast<std::streamsize>(content.size()));
    out.close();
    return true;
}

// Remove recovery file after successful save
// Basarili kaydetmeden sonra kurtarma dosyasini kaldir
void AutoSave::removeRecovery(const std::string& filePath) {
    if (saveDir_.empty()) return;
    std::string path = recoveryPath(filePath);
    std::error_code ec;
    fs::remove(path, ec);
}

// Create a backup copy of a file (file -> file~)
// Dosyanin yedek kopyasini olustur (dosya -> dosya~)
bool AutoSave::createBackup(const std::string& filePath) {
    std::string backupPath = filePath + "~";
    std::error_code ec;

    // Only create backup if original exists and backup doesn't
    // Yalnizca orijinal varsa ve yedek yoksa yedek olustur
    if (!fs::exists(filePath, ec)) return false;
    if (fs::exists(backupPath, ec)) return true; // Already backed up / Zaten yedeklendi

    fs::copy_file(filePath, backupPath, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        LOG_WARN("[AutoSave] Backup failed: ", filePath, " -> ", ec.message());
        return false;
    }
    LOG_DEBUG("[AutoSave] Backup created: ", backupPath);
    return true;
}

// List available recovery files
// Kullanilabilir kurtarma dosyalarini listele
std::vector<RecoveryFile> AutoSave::listRecoveryFiles() const {
    std::vector<RecoveryFile> result;
    if (saveDir_.empty()) return result;

    std::error_code ec;
    if (!fs::is_directory(saveDir_, ec)) return result;

    for (auto& entry : fs::directory_iterator(saveDir_, ec)) {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();

        // Reconstruct original path from filename
        // Dosya adindan orijinal yolu yeniden olustur
        std::string originalPath = name;
        for (char& c : originalPath) {
            if (c == '_') c = '/';
        }

        // Use file_time_type duration count as portable timestamp
        // Tasinabilir zaman damgasi olarak file_time_type sure sayacini kullan
        auto ftime = entry.last_write_time();
        auto ticks = static_cast<long long>(ftime.time_since_epoch().count());

        result.push_back({
            originalPath,
            entry.path().string(),
            std::to_string(ticks)
        });
    }
    return result;
}

// Check if a file has been externally modified since we last opened/saved it
// Dosyanin son acmamizdan/kaydetmemizden beri harici olarak degistirilip degistirilmedigini kontrol et
bool AutoSave::hasExternalChange(const std::string& filePath) const {
    std::error_code ec;
    if (!fs::exists(filePath, ec)) return false;

    auto ftime = fs::last_write_time(filePath, ec);
    if (ec) return false;

    std::lock_guard<std::mutex> lock(mtimeMutex_);
    auto it = fileMtimes_.find(filePath);
    if (it == fileMtimes_.end()) return false;

    return ftime != it->second;
}

// Record the current mtime of a file
// Dosyanin mevcut mtime'ini kaydet
void AutoSave::recordMtime(const std::string& filePath) {
    std::error_code ec;
    if (!fs::exists(filePath, ec)) return;

    auto ftime = fs::last_write_time(filePath, ec);
    if (ec) return;

    std::lock_guard<std::mutex> lock(mtimeMutex_);
    fileMtimes_[filePath] = ftime;
}
