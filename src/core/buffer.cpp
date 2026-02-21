#include "buffer.h"

// Default constructor: PieceTable initializes with one empty line
// Varsayilan kurucu: PieceTable tek bir bos satirla baslatilir
Buffer::Buffer() {
    // PieceTable default constructor handles initialization
    // PieceTable varsayilan kurucu baslatmayi halleder
}

// Insert a character at the given line and column position
// Verilen satir ve sutun konumuna bir karakter ekle
void Buffer::insertChar(int line, int col, char c) {
    if (!pt_.isValidPos(line, col)) return;
    pt_.getLineRef(line).insert(col, 1, c);
}

// Delete a single character at the given line and column
// Verilen satir ve sutundaki tek bir karakteri sil
void Buffer::deleteChar(int line, int col) {
    if (line < 0 || line >= pt_.lineCount()) return;
    auto& l = pt_.getLineRef(line);
    if (col < 0 || col >= static_cast<int>(l.size())) return;
    l.erase(col, 1);
}

// Insert a text string at the given line and column position (handles newlines)
// Verilen satir ve sutun konumuna bir metin dizesi ekle (yeni satirlari isler)
void Buffer::insertText(int line, int col, const std::string& text) {
    if (!pt_.isValidPos(line, col)) return;

    // Simple case: no newlines, just insert inline
    // Basit durum: yeni satir yok, sadece satir icine ekle
    size_t nlPos = text.find('\n');
    if (nlPos == std::string::npos) {
        pt_.getLineRef(line).insert(col, text);
        return;
    }

    // Multi-line insert: split text at newlines
    // Cok satirli ekleme: metni yeni satirlarda bol

    // Save the tail of the current line (text after insertion point)
    // Mevcut satirin kuyrugunu kaydet (ekleme noktasindan sonraki metin)
    std::string tail = pt_.getLine(line).substr(col);

    // Replace current line: keep head + first segment before \n
    // Mevcut satiri degistir: basi tut + \n oncesi ilk segmenti ekle
    std::string firstPart = text.substr(0, nlPos);
    pt_.setLine(line, pt_.getLine(line).substr(0, col) + firstPart);

    // Insert middle lines and final line with tail appended
    // Ara satirlari ve kuyruk eklenmi son satiri ekle
    int insertedLines = 0;
    size_t prevPos = nlPos + 1;
    while (true) {
        size_t nextNl = text.find('\n', prevPos);
        if (nextNl == std::string::npos) {
            // Last segment + original tail
            // Son segment + orijinal kuyruk
            std::string lastPart = text.substr(prevPos);
            pt_.insertLineAt(line + insertedLines + 1, lastPart + tail);
            break;
        }
        // Middle line: text between two newlines
        // Ara satir: iki yeni satir arasindaki metin
        std::string midLine = text.substr(prevPos, nextNl - prevPos);
        pt_.insertLineAt(line + insertedLines + 1, midLine);
        insertedLines++;
        prevPos = nextNl + 1;
    }
}

// Delete text in a range from (lineStart, colStart) to (lineEnd, colEnd)
// (lineStart, colStart) ile (lineEnd, colEnd) arasindaki metni sil
void Buffer::deleteRange(int lineStart, int colStart, int lineEnd, int colEnd) {
    if (lineStart < 0 || lineEnd >= pt_.lineCount() || lineStart > lineEnd) return;

    if (lineStart == lineEnd) {
        auto& l = pt_.getLineRef(lineStart);
        l.erase(colStart, colEnd - colStart);
    } else {
        std::string head = pt_.getLine(lineStart).substr(0, colStart);
        std::string tail = pt_.getLine(lineEnd).substr(colEnd);
        pt_.setLine(lineStart, head + tail);

        // Delete lines from lineEnd down to lineStart+1 (reverse to keep indices stable)
        // lineEnd'den lineStart+1'e satirlari sil (indekslerin kararli kalmasi icin tersten)
        for (int i = lineEnd; i > lineStart; --i) {
            pt_.deleteLine(i);
        }
    }
}

// Split a line into two at the given column (used for Enter key)
// Verilen sutunda satiri ikiye bol (Enter tusu icin kullanilir)
void Buffer::splitLine(int line, int col) {
    if (line < 0 || line >= pt_.lineCount()) return;
    if (col < 0) col = 0;

    std::string content = pt_.getLine(line);
    if (col > static_cast<int>(content.size())) col = static_cast<int>(content.size());

    std::string left = content.substr(0, col);
    std::string right = content.substr(col);

    pt_.setLine(line, left);
    pt_.insertLineAt(line + 1, right);
}

// Join two consecutive lines into one
// Ardisik iki satiri tek satirda birlestir
void Buffer::joinLines(int first, int second) {
    if (first < 0 || second <= first || second >= pt_.lineCount()) return;

    std::string merged = pt_.getLine(first) + pt_.getLine(second);
    pt_.setLine(first, merged);
    pt_.deleteLine(second);
}

// Return a copy of the line content at the given index
// Verilen indeksteki satir iceriginin kopyasini dondur
std::string Buffer::getLine(int line) const {
    return pt_.getLine(line);
}

// Return a mutable reference (COW for original lines)
// Degistirilebilir referans dondur (orijinal satirlar icin COW)
std::string& Buffer::getLineRef(int line) {
    return pt_.getLineRef(line);
}

// Return the total number of lines in the buffer
// Tampondaki toplam satir sayisini dondur
int Buffer::lineCount() const {
    return pt_.lineCount();
}

// Return the number of characters (columns) in a given line
// Verilen satirdaki karakter (sutun) sayisini dondur
int Buffer::columnCount(int line) const {
    return pt_.columnCount(line);
}

// Append a new line at the end of the buffer
// Tamponun sonuna yeni bir satir ekle
void Buffer::insertLine(const std::string& line) {
    pt_.appendLine(line);
}

// Insert a new line at the specified index position
// Belirtilen indeks konumuna yeni bir satir ekle
void Buffer::insertLineAt(int index, const std::string& line) {
    pt_.insertLineAt(index, line);
}

// Delete the line at the given index; PieceTable keeps at least one empty line
// Verilen indeksteki satiri sil; PieceTable en az bir bos satir tutar
void Buffer::deleteLine(int index) {
    pt_.deleteLine(index);
}

// Clear all lines and reset buffer to a single empty line
// Tum satirlari temizle ve tamponu tek bir bos satira sifirla
void Buffer::clear() {
    pt_.clear();
}

// Check whether a (line, col) position is valid within the buffer
// (satir, sutun) konumunun tampon icinde gecerli olup olmadigini kontrol et
bool Buffer::isValidPos(int line, int col) const {
    return pt_.isValidPos(line, col);
}

// Strip trailing carriage return characters from all lines (CRLF -> LF)
// Tum satirlardan sondaki satir basi karakterlerini kaldir (CRLF -> LF)
void Buffer::normalizeNewlines() {
    for (int i = 0; i < pt_.lineCount(); ++i) {
        auto& l = pt_.getLineRef(i);
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
    }
}

// Load lines in bulk from a vector (efficient for file loading)
// Vektordan toplu satir yukleme (dosya yukleme icin verimli)
void Buffer::loadLines(std::vector<std::string>&& lines) {
    pt_.loadLines(std::move(lines));
}

// Get read-only access to the underlying piece table
// Alttaki piece table'a salt okunur erisim al
const PieceTable& Buffer::pieceTable() const {
    return pt_;
}
