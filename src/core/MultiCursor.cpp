// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "MultiCursor.h"
#include "buffer.h"
#include <algorithm>

// Default constructor: start with one primary cursor at (0,0)
// Varsayilan kurucu: (0,0)'da bir birincil imlec ile basla
MultiCursor::MultiCursor() {
    cursors_.push_back({0, 0, false, 0, 0});
}

// Clamp cursor to valid buffer bounds
// Imleci gecerli buffer sinirlarina siristir
void MultiCursor::clamp(CursorEntry& c, const Buffer& buf) const {
    int maxLine = buf.lineCount() - 1;
    if (maxLine < 0) maxLine = 0;
    if (c.line > maxLine) c.line = maxLine;
    if (c.line < 0) c.line = 0;

    int maxCol = buf.columnCount(c.line);
    if (c.col > maxCol) c.col = maxCol;
    if (c.col < 0) c.col = 0;
}

// Add a new cursor at position
// Konuma yeni bir imlec ekle
int MultiCursor::addCursor(int line, int col) {
    cursors_.push_back({line, col, false, 0, 0});
    return static_cast<int>(cursors_.size()) - 1;
}

// Remove a cursor by index (cannot remove primary)
// Dizine gore bir imleci kaldir (birincil kaldirilamaz)
bool MultiCursor::removeCursor(int index) {
    if (index <= 0 || index >= static_cast<int>(cursors_.size())) return false;
    cursors_.erase(cursors_.begin() + index);
    return true;
}

// Clear all secondary cursors
// Tum ikincil imlecleri temizle
void MultiCursor::clearSecondary() {
    if (cursors_.size() > 1) {
        cursors_.resize(1);
    }
}

// Set primary cursor position
// Birincil imlec konumunu ayarla
void MultiCursor::setPrimary(int line, int col) {
    if (cursors_.empty()) {
        cursors_.push_back({line, col, false, 0, 0});
    } else {
        cursors_[0].line = line;
        cursors_[0].col = col;
    }
}

// Move all cursors up
// Tum imlecleri yukari tasi
void MultiCursor::moveAllUp(const Buffer& buf) {
    for (auto& c : cursors_) {
        if (c.line > 0) {
            --c.line;
            clamp(c, buf);
        }
    }
}

// Move all cursors down
// Tum imlecleri asagi tasi
void MultiCursor::moveAllDown(const Buffer& buf) {
    for (auto& c : cursors_) {
        if (c.line < buf.lineCount() - 1) {
            ++c.line;
            clamp(c, buf);
        }
    }
}

// Move all cursors left
// Tum imlecleri sola tasi
void MultiCursor::moveAllLeft(const Buffer& buf) {
    for (auto& c : cursors_) {
        if (c.col > 0) {
            --c.col;
        } else if (c.line > 0) {
            --c.line;
            c.col = buf.columnCount(c.line);
        }
    }
}

// Move all cursors right
// Tum imlecleri saga tasi
void MultiCursor::moveAllRight(const Buffer& buf) {
    for (auto& c : cursors_) {
        if (c.col < buf.columnCount(c.line)) {
            ++c.col;
        } else if (c.line < buf.lineCount() - 1) {
            ++c.line;
            c.col = 0;
        }
    }
}

// Move all cursors to line start
// Tum imlecleri satir basina tasi
void MultiCursor::moveAllToLineStart() {
    for (auto& c : cursors_) {
        c.col = 0;
    }
}

// Move all cursors to line end
// Tum imlecleri satir sonuna tasi
void MultiCursor::moveAllToLineEnd(const Buffer& buf) {
    for (auto& c : cursors_) {
        c.col = buf.columnCount(c.line);
    }
}

// Insert text at all cursor positions
// Tum imlec konumlarina metin ekle
void MultiCursor::insertAtAll(Buffer& buf, const std::string& text) {
    // Sort cursors bottom-to-top, right-to-left so earlier inserts don't shift later ones
    // Imlecleri asagidan yukariya, sagdan sola sirala, boylece onceki eklemeler sonrakileri kaydirmaz
    sort();
    std::vector<size_t> order(cursors_.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [this](size_t a, size_t b) {
        if (cursors_[a].line != cursors_[b].line) return cursors_[a].line > cursors_[b].line;
        return cursors_[a].col > cursors_[b].col;
    });

    for (size_t idx : order) {
        auto& c = cursors_[idx];
        buf.insertText(c.line, c.col, text);

        // Count inserted lines and final column
        // Eklenen satirlari ve son sutunu say
        int lines = 0;
        int col = c.col;
        for (char ch : text) {
            if (ch == '\n') {
                ++lines;
                col = 0;
            } else {
                ++col;
            }
        }
        c.line += lines;
        c.col = col;

        // Adjust cursors above this one for the shift
        // Kayma icin bunun uzerindeki imlecleri ayarla
        for (size_t j = 0; j < cursors_.size(); ++j) {
            if (j == idx) continue;
            auto& other = cursors_[j];
            if (other.line == cursors_[idx].line - lines && other.col > c.col - static_cast<int>(text.size())) {
                // Same line, after insertion point
                // Ayni satir, ekleme noktasindan sonra
                // This is handled implicitly by bottom-to-top order
                // Bu, asagidan yukariya sira ile dolayIi olarak islenir
            }
        }
    }
}

// Backspace at all cursor positions
// Tum imlec konumlarinda geri sil
void MultiCursor::backspaceAtAll(Buffer& buf) {
    // Process from bottom to top
    // Asagidan yukariya isle
    sort();
    for (int i = static_cast<int>(cursors_.size()) - 1; i >= 0; --i) {
        auto& c = cursors_[i];
        if (c.col > 0) {
            buf.deleteChar(c.line, c.col - 1);
            --c.col;
        } else if (c.line > 0) {
            int prevLen = buf.columnCount(c.line - 1);
            buf.joinLines(c.line - 1, c.line);
            --c.line;
            c.col = prevLen;

            // Adjust cursors that were on removed line
            // Silinen satirdaki imlecleri ayarla
            for (int j = i + 1; j < static_cast<int>(cursors_.size()); ++j) {
                if (cursors_[j].line > c.line) {
                    --cursors_[j].line;
                }
            }
        }
    }
}

// Delete at all cursor positions
// Tum imlec konumlarinda sil
void MultiCursor::deleteAtAll(Buffer& buf) {
    // Process from bottom to top
    // Asagidan yukariya isle
    sort();
    for (int i = static_cast<int>(cursors_.size()) - 1; i >= 0; --i) {
        auto& c = cursors_[i];
        if (c.col < buf.columnCount(c.line)) {
            buf.deleteChar(c.line, c.col);
        } else if (c.line + 1 < buf.lineCount()) {
            buf.joinLines(c.line, c.line + 1);

            // Adjust cursors on removed line
            // Silinen satirdaki imlecleri ayarla
            for (int j = i + 1; j < static_cast<int>(cursors_.size()); ++j) {
                if (cursors_[j].line > c.line) {
                    --cursors_[j].line;
                }
            }
        }
    }
}

// Set selection anchor at all cursors
// Tum imleclerde secim baglama noktasi ayarla
void MultiCursor::setAnchorAtAll() {
    for (auto& c : cursors_) {
        c.hasSelection = true;
        c.anchorLine = c.line;
        c.anchorCol = c.col;
    }
}

// Clear selection at all cursors
// Tum imleclerde secimi temizle
void MultiCursor::clearSelectionAtAll() {
    for (auto& c : cursors_) {
        c.hasSelection = false;
    }
}

// Add cursor at next occurrence of word in buffer
// Buffer'da kelimenin sonraki olusumuna imlec ekle
int MultiCursor::addCursorAtNextMatch(const Buffer& buf, const std::string& word) {
    if (word.empty()) return -1;

    // Start searching from the last cursor position
    // Son imlec konumundan aramaya basla
    const auto& last = cursors_.back();
    int startLine = last.line;
    int startCol = last.col + 1;

    for (int line = startLine; line < buf.lineCount(); ++line) {
        const std::string& lineText = buf.getLine(line);
        int fromCol = (line == startLine) ? startCol : 0;

        auto pos = lineText.find(word, fromCol);
        if (pos != std::string::npos) {
            int newCol = static_cast<int>(pos);
            // Check not already a cursor here
            // Burada zaten bir imlec olmadigini kontrol et
            bool exists = false;
            for (auto& c : cursors_) {
                if (c.line == line && c.col == newCol) { exists = true; break; }
            }
            if (!exists) {
                return addCursor(line, newCol);
            }
        }
    }

    // Wrap around from beginning
    // Basindan sar
    for (int line = 0; line <= startLine; ++line) {
        const std::string& lineText = buf.getLine(line);
        int maxCol = (line == startLine) ? startCol : static_cast<int>(lineText.size());

        size_t pos = 0;
        while ((pos = lineText.find(word, pos)) != std::string::npos) {
            int newCol = static_cast<int>(pos);
            if (newCol >= maxCol) break;

            bool exists = false;
            for (auto& c : cursors_) {
                if (c.line == line && c.col == newCol) { exists = true; break; }
            }
            if (!exists) {
                return addCursor(line, newCol);
            }
            ++pos;
        }
    }

    return -1;  // No more matches / Daha fazla esleme yok
}

// Add cursors on each line in range at given column
// Verilen sutunda araliktaki her satira imlec ekle
void MultiCursor::addCursorsOnLines(int startLine, int endLine, int col) {
    for (int line = startLine; line <= endLine; ++line) {
        bool exists = false;
        for (auto& c : cursors_) {
            if (c.line == line && c.col == col) { exists = true; break; }
        }
        if (!exists) {
            addCursor(line, col);
        }
    }
}

// Remove duplicate cursors at same position
// Ayni konumdaki tekrar eden imlecleri kaldir
void MultiCursor::dedup() {
    sort();
    auto it = std::unique(cursors_.begin(), cursors_.end(),
                           [](const CursorEntry& a, const CursorEntry& b) {
                               return a.line == b.line && a.col == b.col;
                           });
    cursors_.erase(it, cursors_.end());
    if (cursors_.empty()) {
        cursors_.push_back({0, 0, false, 0, 0});
    }
}

// Sort cursors by position: top to bottom, left to right
// Imlecleri konuma gore sirala: yukaridan asagiya, soldan saga
void MultiCursor::sort() {
    std::sort(cursors_.begin(), cursors_.end(),
              [](const CursorEntry& a, const CursorEntry& b) {
                  if (a.line != b.line) return a.line < b.line;
                  return a.col < b.col;
              });
}
