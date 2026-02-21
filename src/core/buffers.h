#pragma once
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <mutex>
#include "state.h"
#include "file.h"

// Multi-document manager that holds multiple EditorState instances (tabs).
// Birden fazla EditorState ornegini (sekmeler) tutan coklu belge yoneticisi.
// Thread-safe with mutex protection for all public operations.
// Tum genel islemler icin mutex korumasiyla thread-safe.
class Buffers {
public:
    Buffers();

    // Create a new empty document and return its index
    // Yeni bos bir belge olustur ve indeksini dondur
    size_t newDocument(const std::string& untitledName = "untitled");

    // Open a file into a new buffer (or switch to it if already open)
    // Bir dosyayi yeni buffer'a ac (zaten aciksa ona gec)
    bool openFile(const std::string& path);

    // Save the active buffer to its file path
    // Aktif buffer'i dosya yoluna kaydet
    bool saveActive();

    // Save all open buffers and return the count of successfully saved
    // Tum acik buffer'lari kaydet ve basariyla kaydedilenlerin sayisini dondur
    int saveAll();

    // Close the active buffer (creates new empty doc if last one closed)
    // Aktif buffer'i kapat (sonuncusu kapatilirsa yeni bos belge olusturur)
    bool closeActive();

    // Close a buffer at a specific index
    // Belirli bir indeksteki buffer'i kapat
    bool closeAt(size_t index);

    // Switch to a buffer at a specific index
    // Belirli bir indeksteki buffer'a gec
    bool setActive(size_t index);

    // Switch to the next buffer (wraps around)
    // Sonraki buffer'a gec (basa sarar)
    bool next();

    // Switch to the previous buffer (wraps around)
    // Onceki buffer'a gec (basa sarar)
    bool prev();

    // Get the total number of open buffers
    // Acik buffer'larin toplam sayisini al
    size_t count() const;

    // Get the index of the currently active buffer
    // Su an aktif buffer'in indeksini al
    size_t activeIndex() const;

    // Get a reference to the active editor state (mutable)
    // Aktif editor durumuna referans al (degistirilebilir)
    EditorState& active();

    // Get a reference to the active editor state (read-only)
    // Aktif editor durumuna referans al (salt okunur)
    const EditorState& active() const;

    // Get a reference to a buffer at a specific index (read-only, does NOT change active)
    // Belirli bir indeksteki buffer'a referans al (salt okunur, aktifi DEGISTIRMEZ)
    const EditorState& getStateAt(size_t index) const;

    // Find a buffer by its file path, returns index if found
    // Dosya yoluna gore buffer bul, bulunursa indeksini dondur
    std::optional<size_t> findByPath(const std::string& path) const;

    // Get the display title of a buffer at a given index
    // Verilen indeksteki buffer'in goruntuleme basligini al
    std::string titleOf(size_t index) const;

private:
    mutable std::mutex mutex_;                              // Thread safety lock / Thread guvenligi kilidi
    std::vector<std::unique_ptr<EditorState>> docs_;        // Open documents / Acik belgeler
    size_t active_ = 0;                                     // Active document index / Aktif belge indeksi

    // Extract filename from a full path
    // Tam yoldan dosya adini cikar
    static std::string basename(const std::string& path);
};
