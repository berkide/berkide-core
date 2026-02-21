#pragma once
#include <algorithm>

class Buffer;

// Manages the cursor position within a text buffer.
// Metin buffer'i icindeki imlec konumunu yonetir.
// Handles movement, clamping, and line-wrap navigation.
// Hareketi, sinir kontrolunu ve satir-saran gezinmeyi yonetir.
class Cursor {
public:
    Cursor();

    // Get current line number (0-based)
    // Mevcut satir numarasini al (0 tabanli)
    int getLine() const;

    // Get current column number (0-based)
    // Mevcut sutun numarasini al (0 tabanli)
    int getCol() const;

    // Set cursor to an absolute position (clamped to >= 0)
    // Imleci mutlak bir konuma ayarla (>= 0 olarak sinirlandirilir)
    void setPosition(int line, int col);

    // Move cursor up one line, clamping column to line length
    // Imleci bir satir yukari tasi, sutunu satir uzunluguna sinirla
    void moveUp(const Buffer& buf);

    // Move cursor down one line, clamping column to line length
    // Imleci bir satir asagi tasi, sutunu satir uzunluguna sinirla
    void moveDown(const Buffer& buf);

    // Move cursor left one character, wrapping to previous line end if needed
    // Imleci bir karakter sola tasi, gerekirse onceki satir sonuna sar
    void moveLeft(const Buffer& buf);

    // Move cursor right one character, wrapping to next line start if needed
    // Imleci bir karakter saga tasi, gerekirse sonraki satir basina sar
    void moveRight(const Buffer& buf);

    // Move cursor to the beginning of the current line
    // Imleci mevcut satirin basina tasi
    void moveToLineStart();

    // Move cursor to the end of the current line
    // Imleci mevcut satirin sonuna tasi
    void moveToLineEnd(const Buffer& buf);

    // Clamp cursor position to valid buffer bounds
    // Imlec konumunu gecerli buffer sinirlarina hizala
    void clampToBuffer(const Buffer& buf);

private:
    int line_;  // Current line index / Mevcut satir indeksi
    int col_;   // Current column index / Mevcut sutun indeksi
};
