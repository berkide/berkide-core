#pragma once
#include <string>
#include <optional>
#include <chrono>

class Buffer;

// Result of a file I/O operation (success/failure, message, line count)
// Bir dosya giris/cikis isleminin sonucu (basari/basarisizlik, mesaj, satir sayisi)
struct FileResult {
    bool success;
    std::string message;
    size_t lineCount = 0;
};

// Information about a single file (path, size, modification date)
// Tek bir dosya hakkinda bilgi (yol, boyut, degistirilme tarihi)
struct FileInfo {
    std::string path;          // File path / Dosya yolu
    uintmax_t size = 0;        // Size in bytes / Bayt cinsinden boyut
    std::string modified;      // Last modification time / Son degistirilme zamani
};

// Static file system operations for loading, saving, and managing files.
// Dosya yukleme, kaydetme ve yonetme icin statik dosya sistemi islemleri.
// Platform-independent file I/O through C++ std::filesystem.
// C++ std::filesystem araciligiyla platform bagimsiz dosya giris/cikis.
class FileSystem {
public:
    // Load a file into a buffer (handles UTF-8 BOM, CRLF normalization)
    // Bir dosyayi buffer'a yukle (UTF-8 BOM, CRLF normalizasyonunu yonetir)
    static FileResult loadToBuffer(Buffer& buffer, const std::string& path);

    // Save buffer content to a file
    // Buffer icerigini bir dosyaya kaydet
    static FileResult saveFromBuffer(const Buffer& buffer, const std::string& path);

    // Load entire file as a raw text string
    // Tum dosyayi ham metin dizesi olarak yukle
    static std::optional<std::string> loadTextFile(const std::string& path);

    // Save a raw text string to a file
    // Ham metin dizesini bir dosyaya kaydet
    static bool saveTextFile(const std::string& path, const std::string& content);

    // Check if a file exists at the given path
    // Verilen yolda bir dosyanin var olup olmadigini kontrol et
    static bool exists(const std::string& path);

    // Check if a file is readable
    // Bir dosyanin okunabilir olup olmadigini kontrol et
    static bool isReadable(const std::string& path);

    // Check if a file is writable
    // Bir dosyanin yazilabilir olup olmadigini kontrol et
    static bool isWritable(const std::string& path);

    // Rename a file from oldPath to newPath
    // Bir dosyayi oldPath'ten newPath'e yeniden adlandir
    static bool renameFile(const std::string& oldPath, const std::string& newPath);

    // Delete a file at the given path
    // Verilen yoldaki dosyayi sil
    static bool deleteFile(const std::string& path);

    // Copy a file from source to destination
    // Bir dosyayi kaynaktan hedefe kopyala
    static bool copyFile(const std::string& src, const std::string& dest);

    // Get file info (size, modification time)
    // Dosya bilgisini al (boyut, degistirilme zamani)
    static std::optional<FileInfo> getFileInfo(const std::string& path);

    // Check if a file starts with UTF-8 BOM bytes
    // Bir dosyanin UTF-8 BOM byte'lariyla baslayip baslamadigini kontrol et
    static bool hasUTF8BOM(const std::string& path);
};
