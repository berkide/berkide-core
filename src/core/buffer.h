#pragma once
#include <string>
#include <vector>
#include "PieceTable.h"

// Represents a range of text in the buffer (start/end positions)
// Buffer icindeki bir metin araligini temsil eder (baslangic/bitis konumlari)
struct TextSpan {
    int lineStart, colStart;
    int lineEnd, colEnd;
};

// Core text buffer backed by a line-based piece table.
// Satir tabanli piece table ile desteklenen temel metin buffer'i.
// All text editing operations (insert, delete, split, join) go through this class.
// Tum metin duzenleme islemleri (ekleme, silme, bolme, birlestirme) bu sinif uzerinden yapilir.
// Original file content is stored immutably; edits go to an append-only buffer (COW).
// Orijinal dosya icerigi degistirilemez saklanir; duzenlemeler yalnizca ekleme arabellegine gider (COW).
class Buffer {
public:
    Buffer();

    // Single character insertion at given position
    // Verilen konuma tek karakter ekleme
    void insertChar(int line, int col, char c);

    // Single character deletion at given position
    // Verilen konumdan tek karakter silme
    void deleteChar(int line, int col);

    // Multi-character text insertion at given position
    // Verilen konuma cok karakterli metin ekleme
    void insertText(int line, int col, const std::string& text);

    // Delete a range of text between two positions
    // Iki konum arasindaki metni silme
    void deleteRange(int lineStart, int colStart, int lineEnd, int colEnd);

    // Split a line into two at the given column (Enter key behavior)
    // Verilen sutunda bir satiri ikiye bolme (Enter tusu davranisi)
    void splitLine(int line, int col);

    // Join two consecutive lines into one
    // Ardisik iki satiri birlestirme
    void joinLines(int first, int second);

    // Get a copy of a line's content
    // Bir satirin iceriginin kopyasini alma
    std::string getLine(int line) const;

    // Get a mutable reference to a line (COW: original lines copied to add buffer)
    // Bir satira degistirilebilir referans alma (COW: orijinal satirlar ekleme arabellegine kopyalanir)
    std::string& getLineRef(int line);

    // Total number of lines in the buffer
    // Buffer'daki toplam satir sayisi
    int lineCount() const;

    // Number of characters in a specific line
    // Belirli bir satirdaki karakter sayisi
    int columnCount(int line) const;

    // Append a new line at the end of the buffer
    // Buffer'in sonuna yeni satir ekleme
    void insertLine(const std::string& line);

    // Insert a new line at a specific index
    // Belirli bir indekse yeni satir ekleme
    void insertLineAt(int index, const std::string& line);

    // Delete a line at a specific index
    // Belirli bir indeksteki satiri silme
    void deleteLine(int index);

    // Clear all content, reset to single empty line
    // Tum icerigi temizle, tek bos satira sifirla
    void clear();

    // Check if a (line, col) position is valid within the buffer
    // Bir (satir, sutun) konumunun buffer icinde gecerli olup olmadigini kontrol etme
    bool isValidPos(int line, int col) const;

    // Remove trailing \r from all lines (Windows CRLF -> LF conversion)
    // Tum satirlardaki sondaki \r'yi kaldir (Windows CRLF -> LF donusumu)
    void normalizeNewlines();

    // Load lines in bulk from a vector (for efficient file loading)
    // Bir vektordan toplu satir yukleme (verimli dosya yukleme icin)
    void loadLines(std::vector<std::string>&& lines);

    // Get the underlying piece table (read-only, for diagnostics)
    // Alttaki piece table'a erisin (salt okunur, tanilar icin)
    const PieceTable& pieceTable() const;

private:
    PieceTable pt_;  // Piece table storage (replaces vector<string>) / Piece table depolama (vector<string> yerine)
};
