#pragma once
#include <string>
#include <filesystem>

namespace berkide {

// Manages runtime directory paths for BerkIDE.
// BerkIDE icin calisma zamani dizin yollarini yonetir.
// Detects executable location and user home for .berkide/ directories.
// .berkide/ dizinleri icin calistirilabilir dosya konumunu ve kullanici evini tespit eder.
struct BerkidePaths {
    std::string appRoot;      // Directory where the binary resides / Binary'nin bulundugu dizin
    std::string appBerkide;   // .berkide/ next to the binary / Binary'nin yanindaki .berkide/
    std::string userHome;     // User home directory (~) / Kullanici ev dizini (~)
    std::string userBerkide;  // ~/.berkide/ user configuration / ~/.berkide/ kullanici yapilandirmasi

    // Singleton access to the paths instance
    // Yollar ornegine tekil erisim
    static BerkidePaths& instance();

    // Create required directory structure if it doesn't exist
    // Gerekli dizin yapisini yoksa olustur
    void ensureStructure();

    // Create a directory (and parents) if it doesn't exist
    // Yoksa bir dizin (ve ust dizinleri) olustur
    static void ensureDir(const std::string& path);
};

}
