// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

// A single mark (named position in a buffer)
// Tek bir isaret (buffer'daki adlandirilmis konum)
struct Mark {
    int line = 0;                // Mark line number / Isaret satir numarasi
    int col  = 0;                // Mark column number / Isaret sutun numarasi
};

// An entry in the jump list (position + optional file path for global jumps)
// Atlama listesindeki bir giris (konum + global atlamalar icin istege bagli dosya yolu)
struct JumpEntry {
    std::string filePath;        // File path (empty for same-buffer) / Dosya yolu (ayni buffer icin bos)
    int line = 0;                // Line number / Satir numarasi
    int col  = 0;                // Column number / Sutun numarasi
};

// Manages named marks, jump list, and change list for navigation.
// Gezinme icin adlandirilmis isaretleri, atlama listesini ve degisiklik listesini yonetir.
//
// Mark naming convention (follows Vim):
// Isaret adlandirma kurali (Vim'i takip eder):
//   a-z : Buffer-local marks (per document) / Buffer-yerel isaretler (belge basina)
//   A-Z : Global marks (cross-file, includes file path) / Global isaretler (dosyalar arasi)
//   .   : Last edit position / Son duzenleme konumu
//   '   : Position before last jump / Son atlamadan onceki konum
//   ^   : Last insert position / Son ekleme konumu
class MarkManager {
public:
    MarkManager();

    // Set a named mark at position
    // Konumda adlandirilmis bir isaret ayarla
    void set(const std::string& name, int line, int col,
             const std::string& filePath = "");

    // Get a mark by name (returns nullptr if not set)
    // Isme gore isaret al (ayarlanmamissa nullptr dondurur)
    const Mark* get(const std::string& name) const;

    // Get the file path associated with a global mark (A-Z)
    // Global bir isaretle (A-Z) iliskili dosya yolunu al
    std::string getFilePath(const std::string& name) const;

    // Delete a named mark
    // Adlandirilmis bir isareti sil
    bool remove(const std::string& name);

    // List all marks as name->mark pairs
    // Tum isaretleri isim->isaret cifti olarak listele
    std::vector<std::pair<std::string, Mark>> list() const;

    // Push current position to jump list (called before a big jump)
    // Mevcut konumu atlama listesine it (buyuk bir atlamadan once cagirilir)
    void pushJump(const std::string& filePath, int line, int col);

    // Navigate backward in the jump list
    // Atlama listesinde geri git
    bool jumpBack(JumpEntry& entry);

    // Navigate forward in the jump list
    // Atlama listesinde ileri git
    bool jumpForward(JumpEntry& entry);

    // Record an edit position (for change list and auto-marks)
    // Bir duzenleme konumunu kaydet (degisiklik listesi ve otomatik isaretler icin)
    void recordEdit(int line, int col);

    // Navigate to previous edit position in change list
    // Degisiklik listesinde onceki duzenleme konumuna git
    bool prevChange(JumpEntry& entry);

    // Navigate to next edit position in change list
    // Degisiklik listesinde sonraki duzenleme konumuna git
    bool nextChange(JumpEntry& entry);

    // Adjust all marks after a text edit (insert/delete shifts positions)
    // Bir metin duzenlemesinden sonra tum isaretleri ayarla (ekleme/silme konumlari kaydirir)
    void adjustMarks(int editLine, int editCol, int linesDelta, int colDelta);

    // Clear all marks (buffer-local only, or all including global)
    // Tum isaretleri temizle (yalnizca buffer-yerel veya global dahil tumu)
    void clearLocal();
    void clearAll();

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Mark> marks_;
    std::unordered_map<std::string, std::string> markFiles_; // File path for global marks / Global isaretler icin dosya yolu

    // Jump list: circular buffer with pointer
    // Atlama listesi: isaretcili dairesel tampon
    static constexpr int kMaxJumps = 100;
    std::vector<JumpEntry> jumpList_;
    int jumpPos_ = -1;            // Current position in jump list / Atlama listesindeki mevcut konum

    // Change list: positions where edits occurred
    // Degisiklik listesi: duzenlemelerin gerceklestigi konumlar
    static constexpr int kMaxChanges = 100;
    std::vector<JumpEntry> changeList_;
    int changePos_ = -1;          // Current position in change list / Degisiklik listesindeki mevcut konum
};
