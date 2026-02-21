#include "file.h"
#include "buffer.h"
#include "Logger.h"
#include <fstream>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

// Load a file from disk into a Buffer, handling BOM and CRLF line endings
// Diskten bir dosyayi BOM ve CRLF satir sonlarini isleyerek Buffer'a yukle
FileResult FileSystem::loadToBuffer(Buffer& buffer, const std::string& path) {
    FileResult result{false, "", 0};

    if (!exists(path)) {
        result.message = "Dosya bulunamadı: " + path;
        return result;
    }

    if (!isReadable(path)) {
        result.message = "Dosya okunamıyor: " + path;
        return result;
    }

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        result.message = "Dosya açılamadı: " + path;
        return result;
    }

    buffer.clear();
    std::string line;
    size_t lineCount = 0;

    // UTF-8 BOM kontrolü
    char bom[3];
    file.read(bom, 3);
    if (!(bom[0] == char(0xEF) && bom[1] == char(0xBB) && bom[2] == char(0xBF))) {
        file.seekg(0);
    }

    try {
        while (std::getline(file, line)) {
            // Windows CRLF düzeltmesi
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            buffer.insertLine(line);
            ++lineCount;
        }

        if (lineCount == 0)
            buffer.insertLine("");

        result.success = true;
        result.lineCount = lineCount;
        result.message = "Dosya başarıyla yüklendi.";
    }
    catch (const std::exception& ex) {
        result.success = false;
        result.message = std::string("Dosya okuma hatası: ") + ex.what();
    }

    return result;
}

// Save a Buffer's contents to a file on disk
// Buffer icerigini diskteki bir dosyaya kaydet
FileResult FileSystem::saveFromBuffer(const Buffer& buffer, const std::string& path) {
    FileResult result{false, "", 0};

    std::ofstream file(path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!file.is_open()) {
        result.message = "Dosya yazılamadı: " + path;
        return result;
    }

    try {
        for (int i = 0; i < buffer.lineCount(); ++i) {
            const std::string& line = buffer.getLine(i);
            file.write(line.c_str(), static_cast<std::streamsize>(line.size()));
            file.put('\n');
        }

        file.flush();
        result.success = true;
        result.lineCount = buffer.lineCount();
        result.message = "Dosya başarıyla kaydedildi.";
    }
    catch (const std::exception& ex) {
        result.success = false;
        result.message = std::string("Dosya yazma hatası: ") + ex.what();
    }

    return result;
}

// Read a text file and return its entire content as a string
// Bir metin dosyasini oku ve tum icerigini string olarak dondur
std::optional<std::string> FileSystem::loadTextFile(const std::string& path) {
    try {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            LOG_ERROR("[FileSystem] loadTextFile failed: cannot open ", path);
            return std::nullopt;
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    } catch (const std::exception& e) {
        LOG_ERROR("[FileSystem] Exception in loadTextFile: ", e.what());
        return std::nullopt;
    }
}

// Save a string as a UTF-8 text file (overwrites existing content)
// Bir dizeyi UTF-8 metin dosyasi olarak kaydet (mevcut icerigi uzerine yazar)
bool FileSystem::saveTextFile(const std::string& path, const std::string& content) {
    try {
        std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            LOG_ERROR("[FileSystem] saveTextFile failed: cannot open ", path);
            return false;
        }
        file << content;
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("[FileSystem] Exception in saveTextFile: ", e.what());
        return false;
    }
}

// Check if a file or directory exists at the given path
// Verilen yolda bir dosya veya dizin olup olmadigini kontrol et
bool FileSystem::exists(const std::string& path) {
    return fs::exists(std::filesystem::path(path));
}

// Check if the file at the given path is readable
// Verilen yoldaki dosyanin okunabilir olup olmadigini kontrol et
bool FileSystem::isReadable(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

// Check if the file at the given path is writable
// Verilen yoldaki dosyanin yazilabilir olup olmadigini kontrol et
bool FileSystem::isWritable(const std::string& path) {
    std::ofstream f(path, std::ios::app);
    return f.good();
}

// Rename (move) a file from oldPath to newPath
// Bir dosyayi oldPath'ten newPath'e yeniden adlandir (tasi)
bool FileSystem::renameFile(const std::string& oldPath, const std::string& newPath) {
    try {
        fs::rename(oldPath, newPath);
        return true;
    } catch (...) {
        return false;
    }
}

// Delete a file at the given path
// Verilen yoldaki dosyayi sil
bool FileSystem::deleteFile(const std::string& path) {
    try {
        return fs::remove(path);
    } catch (...) {
        return false;
    }
}

// Copy a file from source path to destination path (overwrites if exists)
// Kaynak yoldan hedef yola bir dosya kopyala (varsa uzerine yazar)
bool FileSystem::copyFile(const std::string& src, const std::string& dest) {
    try {
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

// Retrieve file metadata (path, size, last modification time)
// Dosya meta verilerini al (yol, boyut, son degisiklik zamani)
std::optional<FileInfo> FileSystem::getFileInfo(const std::string& path) {
    try {
        if (!fs::exists(path)) return std::nullopt;
        FileInfo info;
        info.path = path;
        info.size = fs::file_size(path);

        // 🔧 platformdan bağımsız tarih dönüştürme
        auto ftime = fs::last_write_time(path);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - decltype(ftime)::clock::now() + std::chrono::system_clock::now());
        std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);

        // string'e çevir
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&cftime));
        info.modified = buf;

        return info;
    } catch (...) {
        return std::nullopt;
    }
}

// Check if a file starts with a UTF-8 BOM (Byte Order Mark)
// Bir dosyanin UTF-8 BOM (Bayt Sirasi Isareti) ile baslayip baslamadigini kontrol et
bool FileSystem::hasUTF8BOM(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    unsigned char bom[3];
    file.read(reinterpret_cast<char*>(bom), 3);
    return bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF;
}