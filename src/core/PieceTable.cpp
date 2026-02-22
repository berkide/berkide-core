// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "PieceTable.h"
#include <algorithm>
#include <stdexcept>

// Constructor: initialize with a single empty line in the add buffer
// Kurucu: ekleme arabelleginde tek bir bos satirla baslatir
PieceTable::PieceTable() {
    add_.push_back("");
    pieces_.push_back({Source::Add, 0, 1});
    lineCount_ = 1;
}

// Locate the piece and offset within it for a logical line index
// Mantiksal satir indeksi icin parcayi ve icindeki ofset'i bul
PieceTable::PiecePos PieceTable::findLine(int line) const {
    int cumulative = 0;
    for (int i = 0; i < static_cast<int>(pieces_.size()); ++i) {
        if (cumulative + pieces_[i].count > line) {
            return {i, line - cumulative};
        }
        cumulative += pieces_[i].count;
    }
    // Out of range: return last position
    // Aralik disi: son konumu dondur
    if (!pieces_.empty()) {
        int last = static_cast<int>(pieces_.size()) - 1;
        return {last, pieces_[last].count - 1};
    }
    return {0, 0};
}

// Get a const reference to the actual string at a piece position
// Parca konumundaki gercek string'e const referans al
const std::string& PieceTable::lineAtConst(const Piece& piece, int offset) const {
    int idx = piece.start + offset;
    return (piece.source == Source::Original) ? original_[idx] : add_[idx];
}

// Split a piece at the given offset within it
// Parcayi icindeki verilen ofset'te bol
void PieceTable::splitPiece(int pieceIdx, int offset) {
    if (offset <= 0 || offset >= pieces_[pieceIdx].count) return;

    Piece& p = pieces_[pieceIdx];
    Piece second{p.source, p.start + offset, p.count - offset};
    p.count = offset;
    pieces_.insert(pieces_.begin() + pieceIdx + 1, second);
}

// Recalculate the cached line count from all pieces
// Tum parcalardan onbelleklenmis satir sayisini yeniden hesapla
void PieceTable::recalcLineCount() {
    int total = 0;
    for (const auto& p : pieces_) {
        total += p.count;
    }
    lineCount_ = total;
}

// Return a copy of the line at the given index
// Verilen indeksteki satirin kopyasini dondur
std::string PieceTable::getLine(int line) const {
    if (line < 0 || line >= lineCount_) return "";
    auto pos = findLine(line);
    return lineAtConst(pieces_[pos.pieceIdx], pos.offset);
}

// Return a mutable reference with copy-on-write for original lines
// Orijinal satirlar icin yazimda kopyalama ile degistirilebilir referans dondur
std::string& PieceTable::getLineRef(int line) {
    auto pos = findLine(line);
    auto& piece = pieces_[pos.pieceIdx];

    if (piece.source == Source::Original) {
        // Copy-on-write: copy line to add buffer, isolate in pieces
        // Yazimda kopyala: satiri ekleme arabellegine kopyala, parcalarda izole et
        int addIdx = static_cast<int>(add_.size());
        add_.push_back(original_[piece.start + pos.offset]);

        // Split before this line if needed
        // Gerekirse bu satirdan once bol
        if (pos.offset > 0) {
            splitPiece(pos.pieceIdx, pos.offset);
            pos.pieceIdx++;
            pos.offset = 0;
        }

        // Split after this line if needed
        // Gerekirse bu satirdan sonra bol
        auto& p = pieces_[pos.pieceIdx];
        if (p.count > 1) {
            splitPiece(pos.pieceIdx, 1);
        }

        // Replace the single-line piece with an Add piece
        // Tek satirlik parcayi Add parcasiyla degistir
        pieces_[pos.pieceIdx] = {Source::Add, addIdx, 1};
        return add_[addIdx];
    }

    // Already in add buffer, return directly
    // Zaten ekleme arabelleginde, dogrudan dondur
    return add_[piece.start + pos.offset];
}

// Total number of logical lines
// Toplam mantiksal satir sayisi
int PieceTable::lineCount() const {
    return lineCount_;
}

// Number of characters in a given line
// Verilen satirdaki karakter sayisi
int PieceTable::columnCount(int line) const {
    if (line < 0 || line >= lineCount_) return 0;
    auto pos = findLine(line);
    return static_cast<int>(lineAtConst(pieces_[pos.pieceIdx], pos.offset).size());
}

// Insert a new line at the given index
// Verilen indekse yeni satir ekle
void PieceTable::insertLineAt(int index, const std::string& line) {
    if (index < 0) index = 0;
    if (index > lineCount_) index = lineCount_;

    int addIdx = static_cast<int>(add_.size());
    add_.push_back(line);

    if (pieces_.empty() || lineCount_ == 0) {
        // Buffer is empty, just set this as the only piece
        // Arabellek bos, bunu tek parca olarak ayarla
        pieces_.clear();
        pieces_.push_back({Source::Add, addIdx, 1});
        lineCount_ = 1;
        return;
    }

    if (index == lineCount_) {
        // Append at end: check if we can extend the last piece
        // Sona ekle: son parcayi genisletebilir miyiz kontrol et
        auto& last = pieces_.back();
        if (last.source == Source::Add && last.start + last.count == addIdx) {
            last.count++;
        } else {
            pieces_.push_back({Source::Add, addIdx, 1});
        }
        lineCount_++;
        return;
    }

    // Insert in the middle: find the piece and split
    // Ortaya ekle: parcayi bul ve bol
    auto pos = findLine(index);

    if (pos.offset > 0) {
        splitPiece(pos.pieceIdx, pos.offset);
        pos.pieceIdx++;
    }

    pieces_.insert(pieces_.begin() + pos.pieceIdx, {Source::Add, addIdx, 1});
    lineCount_++;
}

// Append a line at the end
// Sona satir ekle
void PieceTable::appendLine(const std::string& line) {
    insertLineAt(lineCount_, line);
}

// Delete the line at the given index
// Verilen indeksteki satiri sil
void PieceTable::deleteLine(int index) {
    if (index < 0 || index >= lineCount_) return;

    auto pos = findLine(index);
    auto& piece = pieces_[pos.pieceIdx];

    if (piece.count == 1) {
        // Remove entire piece
        // Tum parcayi kaldir
        pieces_.erase(pieces_.begin() + pos.pieceIdx);
    } else if (pos.offset == 0) {
        // First line of piece: adjust start and count
        // Parcanin ilk satiri: baslangic ve sayiyi ayarla
        piece.start++;
        piece.count--;
    } else if (pos.offset == piece.count - 1) {
        // Last line of piece: adjust count
        // Parcanin son satiri: sayiyi ayarla
        piece.count--;
    } else {
        // Middle of piece: split and remove
        // Parcanin ortasi: bol ve kaldir
        splitPiece(pos.pieceIdx, pos.offset);
        auto& second = pieces_[pos.pieceIdx + 1];
        second.start++;
        second.count--;
        if (second.count == 0) {
            pieces_.erase(pieces_.begin() + pos.pieceIdx + 1);
        }
    }

    lineCount_--;

    // Keep at least one empty line
    // En az bir bos satir tut
    if (lineCount_ == 0) {
        add_.push_back("");
        pieces_.push_back({Source::Add, static_cast<int>(add_.size()) - 1, 1});
        lineCount_ = 1;
    }
}

// Replace the content of a line (COW for original lines)
// Bir satirin icerigini degistir (orijinal satirlar icin COW)
void PieceTable::setLine(int index, const std::string& content) {
    if (index < 0 || index >= lineCount_) return;
    std::string& ref = getLineRef(index);
    ref = content;
}

// Check if (line, col) is a valid position
// (satir, sutun) gecerli bir konum mu kontrol et
bool PieceTable::isValidPos(int line, int col) const {
    if (line < 0 || line >= lineCount_) return false;
    auto pos = findLine(line);
    int len = static_cast<int>(lineAtConst(pieces_[pos.pieceIdx], pos.offset).size());
    return col >= 0 && col <= len;
}

// Load lines in bulk (move), replacing all content
// Toplu satir yukleme (tasima), tum icerigi degistirir
void PieceTable::loadLines(std::vector<std::string>&& lines) {
    original_ = std::move(lines);
    add_.clear();
    pieces_.clear();

    if (original_.empty()) {
        original_.push_back("");
    }

    pieces_.push_back({Source::Original, 0, static_cast<int>(original_.size())});
    lineCount_ = static_cast<int>(original_.size());
}

// Load lines in bulk (copy), replacing all content
// Toplu satir yukleme (kopyalama), tum icerigi degistirir
void PieceTable::loadLines(const std::vector<std::string>& lines) {
    original_ = lines;
    add_.clear();
    pieces_.clear();

    if (original_.empty()) {
        original_.push_back("");
    }

    pieces_.push_back({Source::Original, 0, static_cast<int>(original_.size())});
    lineCount_ = static_cast<int>(original_.size());
}

// Clear all content, reset to single empty line
// Tum icerigi temizle, tek bos satira sifirla
void PieceTable::clear() {
    original_.clear();
    add_.clear();
    add_.push_back("");
    pieces_.clear();
    pieces_.push_back({Source::Add, 0, 1});
    lineCount_ = 1;
}

// Get all lines as a materialized vector
// Tum satirlari somutlastirilmis vektor olarak al
std::vector<std::string> PieceTable::allLines() const {
    std::vector<std::string> result;
    result.reserve(lineCount_);
    for (const auto& piece : pieces_) {
        const auto& buf = (piece.source == Source::Original) ? original_ : add_;
        for (int i = 0; i < piece.count; ++i) {
            result.push_back(buf[piece.start + i]);
        }
    }
    return result;
}

// Get the number of pieces (for diagnostics)
// Parca sayisini al (tanilar icin)
int PieceTable::pieceCount() const {
    return static_cast<int>(pieces_.size());
}

// Merge adjacent pieces from the same source when contiguous
// Bitisik oldugunda ayni kaynaktan parcalari birlestir
void PieceTable::compact() {
    if (pieces_.size() <= 1) return;

    std::vector<Piece> merged;
    merged.push_back(pieces_[0]);

    for (size_t i = 1; i < pieces_.size(); ++i) {
        auto& prev = merged.back();
        const auto& cur = pieces_[i];

        if (prev.source == cur.source && prev.start + prev.count == cur.start) {
            // Contiguous pieces from same source: merge
            // Ayni kaynaktan bitisik parcalar: birlestir
            prev.count += cur.count;
        } else {
            merged.push_back(cur);
        }
    }

    pieces_ = std::move(merged);
}
