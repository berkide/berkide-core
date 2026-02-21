#pragma once
#include <vector>
#include <string>
#include <mutex>

class Buffer;

// A single cursor with optional selection anchor
// Istege bagli secim baglama noktasli tek bir imlec
struct CursorEntry {
    int line = 0;                // Cursor line / Imlec satiri
    int col  = 0;                // Cursor column / Imlec sutunu
    bool hasSelection = false;   // Whether selection is active / Secim aktif mi
    int anchorLine = 0;          // Selection anchor line / Secim baglama satiri
    int anchorCol  = 0;          // Selection anchor column / Secim baglama sutunu
};

// Manages multiple cursors for simultaneous editing.
// Eszamanli duzenleme icin birden fazla imleci yonetir.
// Each cursor can independently have a selection. Operations are applied to all cursors.
// Her imlec bagimsiz olarak bir secime sahip olabilir. Islemler tum imleclere uygulanir.
class MultiCursor {
public:
    MultiCursor();

    // Add a new cursor at position
    // Konuma yeni bir imlec ekle
    int addCursor(int line, int col);

    // Remove a cursor by index
    // Dizine gore bir imleci kaldir
    bool removeCursor(int index);

    // Clear all cursors except the primary (index 0)
    // Birincil (dizin 0) haricindeki tum imlecleri temizle
    void clearSecondary();

    // Get all cursors
    // Tum imlecleri al
    const std::vector<CursorEntry>& cursors() const { return cursors_; }

    // Get cursor count
    // Imlec sayisini al
    int count() const { return static_cast<int>(cursors_.size()); }

    // Check if multi-cursor mode is active (more than 1 cursor)
    // Coklu imlec modunun aktif olup olmadigini kontrol et (1'den fazla imlec)
    bool isActive() const { return cursors_.size() > 1; }

    // Set primary cursor position (index 0)
    // Birincil imlec konumunu ayarla (dizin 0)
    void setPrimary(int line, int col);

    // Get primary cursor
    // Birincil imleci al
    const CursorEntry& primary() const { return cursors_[0]; }

    // Move all cursors in a direction
    // Tum imlecleri bir yonde tasi
    void moveAllUp(const Buffer& buf);
    void moveAllDown(const Buffer& buf);
    void moveAllLeft(const Buffer& buf);
    void moveAllRight(const Buffer& buf);
    void moveAllToLineStart();
    void moveAllToLineEnd(const Buffer& buf);

    // Insert text at all cursor positions (adjusts subsequent cursor positions)
    // Tum imlec konumlarina metin ekle (sonraki imlec konumlarini ayarlar)
    void insertAtAll(Buffer& buf, const std::string& text);

    // Delete character before all cursors (backspace)
    // Tum imleclerden onceki karakteri sil (geri al)
    void backspaceAtAll(Buffer& buf);

    // Delete character at all cursors (delete key)
    // Tum imleclerdeki karakteri sil (silme tusu)
    void deleteAtAll(Buffer& buf);

    // Set selection anchor at all cursors
    // Tum imleclerde secim baglama noktasi ayarla
    void setAnchorAtAll();

    // Clear selection at all cursors
    // Tum imleclerde secimi temizle
    void clearSelectionAtAll();

    // Add cursor at next occurrence of current word/selection
    // Mevcut kelime/secimin sonraki olusumuna imlec ekle
    int addCursorAtNextMatch(const Buffer& buf, const std::string& word);

    // Add cursors on each selected line (column mode)
    // Secili her satira imlec ekle (sutun modu)
    void addCursorsOnLines(int startLine, int endLine, int col);

    // Remove duplicate cursors (same position)
    // Tekrar eden imlecleri kaldir (ayni konum)
    void dedup();

    // Sort cursors by position (top to bottom, left to right)
    // Imlecleri konuma gore sirala (yukaridan asagiya, soldan saga)
    void sort();

private:
    std::vector<CursorEntry> cursors_;

    // Clamp a cursor to valid buffer bounds
    // Bir imleci gecerli buffer sinirlarina siristir
    void clamp(CursorEntry& c, const Buffer& buf) const;
};
