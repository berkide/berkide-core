#include "Selection.h"
#include "buffer.h"
#include <algorithm>

// Default constructor: selection starts inactive
// Varsayilan kurucu: secim etkin degil olarak baslar
Selection::Selection() = default;

// Activate selection and set anchor point
// Secimi etkinlestir ve baglama noktasini ayarla
void Selection::setAnchor(int line, int col) {
    anchorLine_ = line;
    anchorCol_  = col;
    active_     = true;
}

// Deactivate selection
// Secimi devre disi birak
void Selection::clear() {
    active_ = false;
    anchorLine_ = 0;
    anchorCol_  = 0;
    type_ = SelectionType::Char;
}

// Get the normalized (ordered) selection range.
// Normalize edilmis (siralanmis) secim araligini al.
// Ensures startLine:startCol <= endLine:endCol regardless of anchor vs cursor order.
// Baglama ve imlec sirasina bakilmaksizin startLine:startCol <= endLine:endCol saglar.
void Selection::getRange(int cursorLine, int cursorCol,
                         int& startLine, int& startCol,
                         int& endLine, int& endCol) const {
    if (!active_) {
        startLine = startCol = endLine = endCol = 0;
        return;
    }

    if (type_ == SelectionType::Line) {
        // Line-wise: always full lines, columns are 0 to end
        // Satir bazli: her zaman tam satirlar, sutunlar 0'dan sona
        startLine = std::min(anchorLine_, cursorLine);
        endLine   = std::max(anchorLine_, cursorLine);
        startCol  = 0;
        endCol    = 0; // Caller should treat endLine as inclusive full line
        return;
    }

    // Character-wise and block: compare anchor vs cursor
    // Karakter bazli ve blok: baglama ile imleci karsilastir
    bool anchorFirst = (anchorLine_ < cursorLine) ||
                       (anchorLine_ == cursorLine && anchorCol_ <= cursorCol);

    if (anchorFirst) {
        startLine = anchorLine_;
        startCol  = anchorCol_;
        endLine   = cursorLine;
        endCol    = cursorCol;
    } else {
        startLine = cursorLine;
        startCol  = cursorCol;
        endLine   = anchorLine_;
        endCol    = anchorCol_;
    }

    if (type_ == SelectionType::Block) {
        // Block selection: normalize columns independently of line order
        // Blok secim: sutunlari satir sirasindan bagimsiz olarak normalize et
        startCol = std::min(anchorCol_, cursorCol);
        endCol   = std::max(anchorCol_, cursorCol);
    }
}

// Extract selected text from buffer (character-wise)
// Buffer'dan secili metni cikar (karakter bazli)
std::string Selection::getText(const Buffer& buf, int cursorLine, int cursorCol) const {
    if (!active_) return "";

    int sLine, sCol, eLine, eCol;
    getRange(cursorLine, cursorCol, sLine, sCol, eLine, eCol);

    // Clamp to buffer bounds
    // Buffer sinirlarina hizala
    int maxLine = buf.lineCount() - 1;
    if (maxLine < 0) return "";
    sLine = std::clamp(sLine, 0, maxLine);
    eLine = std::clamp(eLine, 0, maxLine);

    if (type_ == SelectionType::Line) {
        // Line-wise: return full lines with trailing newlines
        // Satir bazli: sondaki yeni satirlarla tam satirlari dondur
        std::string result;
        for (int i = sLine; i <= eLine; ++i) {
            result += buf.getLine(i);
            result += '\n';
        }
        return result;
    }

    if (type_ == SelectionType::Block) {
        // Block selection: extract column range from each line
        // Blok secim: her satirdan sutun araligini cikar
        std::string result;
        for (int i = sLine; i <= eLine; ++i) {
            std::string line = buf.getLine(i);
            int lineLen = static_cast<int>(line.size());
            int cs = std::min(sCol, lineLen);
            int ce = std::min(eCol, lineLen);
            if (cs < ce) {
                result += line.substr(cs, ce - cs);
            }
            if (i < eLine) result += '\n';
        }
        return result;
    }

    // Character-wise selection
    // Karakter bazli secim
    if (sLine == eLine) {
        // Single line selection
        // Tek satirlik secim
        std::string line = buf.getLine(sLine);
        int lineLen = static_cast<int>(line.size());
        int cs = std::clamp(sCol, 0, lineLen);
        int ce = std::clamp(eCol, 0, lineLen);
        return line.substr(cs, ce - cs);
    }

    // Multi-line selection
    // Cok satirlik secim
    std::string result;

    // First line: from startCol to end of line
    // Ilk satir: startCol'dan satir sonuna
    std::string firstLine = buf.getLine(sLine);
    int firstLen = static_cast<int>(firstLine.size());
    int cs = std::clamp(sCol, 0, firstLen);
    result += firstLine.substr(cs);
    result += '\n';

    // Middle lines: full lines
    // Ortadaki satirlar: tam satirlar
    for (int i = sLine + 1; i < eLine; ++i) {
        result += buf.getLine(i);
        result += '\n';
    }

    // Last line: from start to endCol
    // Son satir: baslangictan endCol'a
    std::string lastLine = buf.getLine(eLine);
    int lastLen = static_cast<int>(lastLine.size());
    int ce = std::clamp(eCol, 0, lastLen);
    result += lastLine.substr(0, ce);

    return result;
}

// Get selected text as complete lines (for line-wise operations)
// Secili metni tam satirlar olarak al (satir bazli islemler icin)
std::string Selection::getLineText(const Buffer& buf, int cursorLine, int cursorCol) const {
    if (!active_) return "";

    int sLine, sCol, eLine, eCol;
    getRange(cursorLine, cursorCol, sLine, sCol, eLine, eCol);

    int maxLine = buf.lineCount() - 1;
    if (maxLine < 0) return "";
    sLine = std::clamp(sLine, 0, maxLine);
    eLine = std::clamp(eLine, 0, maxLine);

    // Always return full lines regardless of selection type
    // Secim turune bakilmaksizin her zaman tam satirlari dondur
    std::string result;
    for (int i = sLine; i <= eLine; ++i) {
        result += buf.getLine(i);
        result += '\n';
    }
    return result;
}
