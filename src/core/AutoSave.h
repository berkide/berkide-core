#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <vector>
#include <filesystem>
#include <unordered_map>

class Buffers;
class EventBus;

// Auto-save recovery file information
// Otomatik kaydetme kurtarma dosyasi bilgisi
struct RecoveryFile {
    std::string originalPath;    // Original file path / Orijinal dosya yolu
    std::string recoveryPath;    // Auto-save recovery path / Otomatik kaydetme kurtarma yolu
    std::string timestamp;       // When the auto-save was created / Otomatik kaydetme ne zaman olusturuldu
};

// Automatic save, backup, and file change detection system.
// Otomatik kaydetme, yedekleme ve dosya degisikligi algilama sistemi.
// Periodically saves modified buffers to ~/.berkide/autosave/ directory.
// Degistirilmis buffer'lari periyodik olarak ~/.berkide/autosave/ dizinine kaydeder.
// Creates backup files (file~) before first write.
// Ilk yazmadan once yedek dosyalar (dosya~) olusturur.
// Detects external file modifications by checking mtime.
// mtime kontrol ederek harici dosya degisikliklerini algilar.
class AutoSave {
public:
    AutoSave();
    ~AutoSave();

    // Set the auto-save directory path
    // Otomatik kaydetme dizin yolunu ayarla
    void setDirectory(const std::string& dir) { saveDir_ = dir; }

    // Set the auto-save interval in seconds (0 = disabled)
    // Otomatik kaydetme araligini saniye olarak ayarla (0 = devre disi)
    void setInterval(int seconds) { intervalSec_ = seconds; }

    // Set references to buffers and event bus
    // Buffer'lar ve olay veri yoluna referanslari ayarla
    void setBuffers(Buffers* bufs) { buffers_ = bufs; }
    void setEventBus(EventBus* eb) { eventBus_ = eb; }

    // Start the auto-save background thread
    // Otomatik kaydetme arka plan thread'ini baslat
    void start();

    // Stop the auto-save background thread
    // Otomatik kaydetme arka plan thread'ini durdur
    void stop();

    // Create a backup of a file before first write (file -> file~)
    // Ilk yazmadan once dosyanin yedegini olustur (dosya -> dosya~)
    bool createBackup(const std::string& filePath);

    // Save a buffer's content to the auto-save directory
    // Bir buffer'in icerigini otomatik kaydetme dizinine kaydet
    bool saveBuffer(const std::string& filePath, const std::string& content);

    // Remove auto-save file (called after successful save)
    // Otomatik kaydetme dosyasini kaldir (basarili kaydetmeden sonra cagirilir)
    void removeRecovery(const std::string& filePath);

    // List available recovery files
    // Kullanilabilir kurtarma dosyalarini listele
    std::vector<RecoveryFile> listRecoveryFiles() const;

    // Check if a file has been modified externally (by comparing mtime)
    // Bir dosyanin harici olarak degistirilip degistirilmedigini kontrol et (mtime karsilastirarak)
    bool hasExternalChange(const std::string& filePath) const;

    // Record the mtime of a file (called after open/save)
    // Bir dosyanin mtime'ini kaydet (acma/kaydetme sonrasi cagirilir)
    void recordMtime(const std::string& filePath);

private:
    // Background thread function that periodically saves modified buffers
    // Degistirilmis buffer'lari periyodik olarak kaydeden arka plan thread fonksiyonu
    void autoSaveLoop();

    // Generate the recovery file path for a given original path
    // Verilen orijinal yol icin kurtarma dosyasi yolunu olustur
    std::string recoveryPath(const std::string& filePath) const;

    std::string saveDir_;
    int intervalSec_ = 30;       // Default: save every 30 seconds / Varsayilan: her 30 saniyede kaydet
    Buffers* buffers_ = nullptr;
    EventBus* eventBus_ = nullptr;

    std::atomic<bool> running_{false};
    std::thread saveThread_;

    mutable std::mutex mtimeMutex_;
    std::unordered_map<std::string, std::filesystem::file_time_type> fileMtimes_; // Path -> mtime / Yol -> mtime
};
