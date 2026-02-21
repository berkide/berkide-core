#include "cursor.h"
#include "buffer.h"

// Default constructor: initialize cursor at position (0, 0)
// Varsayilan kurucu: imleci (0, 0) konumunda baslat
Cursor::Cursor() : line_(0), col_(0) {}

// Get the current line and column positions of the cursor
// Imlecin mevcut satir ve sutun konumlarini al
int Cursor::getLine() const { return line_; }
int Cursor::getCol() const { return col_; }

// Set cursor to an absolute position, clamping to non-negative values
// Imleci mutlak konuma ayarla, negatif olmayan degerlere sinirla
void Cursor::setPosition(int l, int c) {
    line_ = std::max(0, l);
    col_ = std::max(0, c);
}

// Move cursor one line up, adjusting column if line is shorter
// Imleci bir satir yukari tasi, satir kisaysa sutunu ayarla
void Cursor::moveUp(const Buffer& buf) {
    if (line_ > 0) line_--;
    int len = buf.columnCount(line_);
    if (col_ > len) col_ = len;
}

// Move cursor one line down, adjusting column if line is shorter
// Imleci bir satir asagi tasi, satir kisaysa sutunu ayarla
void Cursor::moveDown(const Buffer& buf) {
    if (line_ + 1 < buf.lineCount()) line_++;
    int len = buf.columnCount(line_);
    if (col_ > len) col_ = len;
}

// Move cursor one column left; wraps to end of previous line if at column 0
// Imleci bir sutun sola tasi; sutun 0 ise onceki satirin sonuna sar
void Cursor::moveLeft(const Buffer& buf) {
    if (col_ > 0) col_--;
    else if (line_ > 0) {
        line_--;
        col_ = buf.columnCount(line_);
    }
}

// Move cursor one column right; wraps to start of next line if at end
// Imleci bir sutun saga tasi; sondaysa sonraki satirin basina sar
void Cursor::moveRight(const Buffer& buf) {
    int len = buf.columnCount(line_);
    if (col_ < len) col_++;
    else if (line_ + 1 < buf.lineCount()) {
        line_++;
        col_ = 0;
    }
}

// Move cursor to the beginning of the current line (column 0)
// Imleci mevcut satirin basina tasi (sutun 0)
void Cursor::moveToLineStart() {
    col_ = 0;
}

// Move cursor to the end of the current line
// Imleci mevcut satirin sonuna tasi
void Cursor::moveToLineEnd(const Buffer& buf) {
    col_ = buf.columnCount(line_);
}

// Clamp cursor position to stay within valid buffer boundaries
// Imleç konumunu gecerli tampon sinirlarinda tutmak icin sinirla
void Cursor::clampToBuffer(const Buffer& buf) {
    if (line_ < 0) line_ = 0;
    if (line_ >= buf.lineCount()) line_ = buf.lineCount() - 1;
    int len = buf.columnCount(line_);
    if (col_ < 0) col_ = 0;
    if (col_ > len) col_ = len;
}
