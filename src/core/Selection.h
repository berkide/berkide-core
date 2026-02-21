#pragma once
#include <string>

class Buffer;

// Selection type: character-wise, line-wise, or block (column) selection
// Secim turu: karakter bazli, satir bazli veya blok (sutun) secimi
enum class SelectionType { Char, Line, Block };

// Represents a text selection (region) within a single buffer.
// Tek bir buffer icindeki bir metin secimini (bolge) temsil eder.
// Stores an anchor point (where selection started) and relies on
// the cursor position as the active end of the selection.
// Bir baglama noktasi (secimin basladigi yer) saklar ve
// imlec konumunu secimin aktif ucu olarak kullanir.
class Selection {
public:
    Selection();

    // Activate selection at the given anchor position
    // Verilen baglama noktasinda secimi etkinlestir
    void setAnchor(int line, int col);

    // Get anchor position
    // Baglama noktasi konumunu al
    int anchorLine() const { return anchorLine_; }
    int anchorCol() const { return anchorCol_; }

    // Check if selection is active
    // Secimin etkin olup olmadigini kontrol et
    bool isActive() const { return active_; }

    // Clear selection (deactivate)
    // Secimi temizle (devre disi birak)
    void clear();

    // Set selection type (char/line/block)
    // Secim turunu ayarla (karakter/satir/blok)
    void setType(SelectionType type) { type_ = type; }

    // Get selection type
    // Secim turunu al
    SelectionType type() const { return type_; }

    // Get the ordered (normalized) selection range given cursor position.
    // Imlec konumuna gore siralanmis (normalize edilmis) secim araligini al.
    // Returns start and end positions where start <= end.
    // Baslangic <= bitis olan baslangic ve bitis konumlarini dondurur.
    void getRange(int cursorLine, int cursorCol,
                  int& startLine, int& startCol,
                  int& endLine, int& endCol) const;

    // Extract selected text from a buffer given cursor position
    // Imlec konumuna gore buffer'dan secili metni cikar
    std::string getText(const Buffer& buf, int cursorLine, int cursorCol) const;

    // Get selected text as lines for line-wise operations
    // Satir bazli islemler icin secili metni satirlar olarak al
    std::string getLineText(const Buffer& buf, int cursorLine, int cursorCol) const;

private:
    int anchorLine_ = 0;         // Selection anchor line / Secim baglama satiri
    int anchorCol_  = 0;         // Selection anchor column / Secim baglama sutunu
    bool active_    = false;     // Whether selection is active / Secimin etkin olup olmadigi
    SelectionType type_ = SelectionType::Char;  // Selection type / Secim turu
};
