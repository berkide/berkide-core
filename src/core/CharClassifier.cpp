// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "CharClassifier.h"
#include "buffer.h"
#include <cctype>
#include <algorithm>

// Constructor: set up default bracket pairs
// Kurucu: varsayilan parantez ciftlerini ayarla
CharClassifier::CharClassifier() {
    addBracketPair('(', ')');
    addBracketPair('[', ']');
    addBracketPair('{', '}');
    addBracketPair('<', '>');
}

// Classify a character into a type
// Bir karakteri bir tipe siniflandir
CharType CharClassifier::classify(char c) const {
    if (c == '\n' || c == '\r') return CharType::LineBreak;
    if (c == ' ' || c == '\t' || c == '\f' || c == '\v') return CharType::Whitespace;
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') return CharType::Word;
    if (extraWordChars_.count(c)) return CharType::Word;
    if (std::ispunct(static_cast<unsigned char>(c))) return CharType::Punctuation;
    // High bytes (>127) treated as word characters (Unicode letters)
    // Yuksek baytlar (>127) kelime karakterleri olarak islenir (Unicode harfler)
    if (static_cast<unsigned char>(c) > 127) return CharType::Word;
    return CharType::Other;
}

// Check if character is a word character
// Karakterin kelime karakteri olup olmadigini kontrol et
bool CharClassifier::isWord(char c) const {
    return classify(c) == CharType::Word;
}

// Check if character is whitespace
// Karakterin bosluk olup olmadigini kontrol et
bool CharClassifier::isWhitespace(char c) const {
    return classify(c) == CharType::Whitespace;
}

// Check if character is any bracket
// Karakterin herhangi bir parantez olup olmadigini kontrol et
bool CharClassifier::isBracket(char c) const {
    return openToClose_.count(c) || closeToOpen_.count(c);
}

// Check if character is an opening bracket
// Karakterin acma parantezi olup olmadigini kontrol et
bool CharClassifier::isOpenBracket(char c) const {
    return openToClose_.count(c) > 0;
}

// Check if character is a closing bracket
// Karakterin kapama parantezi olup olmadigini kontrol et
bool CharClassifier::isCloseBracket(char c) const {
    return closeToOpen_.count(c) > 0;
}

// Get the matching bracket character
// Eslesen parantez karakterini al
char CharClassifier::matchingBracket(char c) const {
    auto it1 = openToClose_.find(c);
    if (it1 != openToClose_.end()) return it1->second;
    auto it2 = closeToOpen_.find(c);
    if (it2 != closeToOpen_.end()) return it2->second;
    return 0;
}

// Find word boundaries at the given column in a line
// Verilen sutunda satirdaki kelime sinirlarini bul
WordRange CharClassifier::wordAt(const std::string& line, int col) const {
    WordRange result{col, col, ""};
    if (line.empty() || col < 0 || col >= static_cast<int>(line.size())) return result;

    CharType type = classify(line[col]);
    if (type == CharType::Whitespace) return result;

    // Scan left to find word start
    // Kelime baslangicini bulmak icin sola tara
    int start = col;
    while (start > 0 && classify(line[start - 1]) == type) --start;

    // Scan right to find word end
    // Kelime sonunu bulmak icin saga tara
    int end = col + 1;
    while (end < static_cast<int>(line.size()) && classify(line[end]) == type) ++end;

    result.startCol = start;
    result.endCol = end;
    result.text = line.substr(start, end - start);
    return result;
}

// Find start of next word (skip current word + whitespace)
// Sonraki kelimenin baslangicini bul (mevcut kelime + boslugu atla)
int CharClassifier::nextWordStart(const std::string& line, int col) const {
    int len = static_cast<int>(line.size());
    if (col >= len) return len;

    // Skip current word characters
    // Mevcut kelime karakterlerini atla
    CharType curType = classify(line[col]);
    while (col < len && classify(line[col]) == curType) ++col;

    // Skip whitespace
    // Bosluklari atla
    while (col < len && isWhitespace(line[col])) ++col;

    return col;
}

// Find start of previous word
// Onceki kelimenin baslangicini bul
int CharClassifier::prevWordStart(const std::string& line, int col) const {
    if (col <= 0) return 0;
    --col;

    // Skip whitespace backward
    // Bosluklari geriye dogru atla
    while (col > 0 && isWhitespace(line[col])) --col;

    // Find start of the word
    // Kelimenin baslangicini bul
    CharType type = classify(line[col]);
    while (col > 0 && classify(line[col - 1]) == type) --col;

    return col;
}

// Find end of current/next word
// Mevcut/sonraki kelimenin sonunu bul
int CharClassifier::wordEnd(const std::string& line, int col) const {
    int len = static_cast<int>(line.size());
    if (col >= len) return len;

    // If on whitespace, skip to next word first
    // Boslukta ise once sonraki kelimeye atla
    while (col < len && isWhitespace(line[col])) ++col;
    if (col >= len) return len;

    // Scan to end of word
    // Kelime sonuna kadar tara
    CharType type = classify(line[col]);
    while (col < len && classify(line[col]) == type) ++col;

    return col;
}

// Scan buffer for a matching bracket in the given direction
// Verilen yonde buffer'da eslesen parantezi tara
BracketMatch CharClassifier::scanForBracket(const Buffer& buf, int line, int col,
                                             char target, char self, ScanDir dir) const {
    int depth = 1;
    int maxLines = buf.lineCount();
    // Limit scan to 10000 lines for performance
    // Performans icin taramayi 10000 satirla sinirla
    int scanned = 0;
    const int MAX_SCAN = 10000;

    if (dir == ScanDir::Forward) {
        std::string curLine = buf.getLine(line);
        for (int c = col + 1; c < static_cast<int>(curLine.size()); ++c) {
            if (curLine[c] == self) ++depth;
            else if (curLine[c] == target) { --depth; if (depth == 0) return {true, line, c, target}; }
        }
        for (int l = line + 1; l < maxLines && scanned < MAX_SCAN; ++l, ++scanned) {
            std::string ln = buf.getLine(l);
            for (int c = 0; c < static_cast<int>(ln.size()); ++c) {
                if (ln[c] == self) ++depth;
                else if (ln[c] == target) { --depth; if (depth == 0) return {true, l, c, target}; }
            }
        }
    } else {
        std::string curLine = buf.getLine(line);
        for (int c = col - 1; c >= 0; --c) {
            if (curLine[c] == self) ++depth;
            else if (curLine[c] == target) { --depth; if (depth == 0) return {true, line, c, target}; }
        }
        for (int l = line - 1; l >= 0 && scanned < MAX_SCAN; --l, ++scanned) {
            std::string ln = buf.getLine(l);
            for (int c = static_cast<int>(ln.size()) - 1; c >= 0; --c) {
                if (ln[c] == self) ++depth;
                else if (ln[c] == target) { --depth; if (depth == 0) return {true, l, c, target}; }
            }
        }
    }

    return {};
}

// Find matching bracket at given position in buffer
// Buffer'daki verilen konumda eslesen parantezi bul
BracketMatch CharClassifier::findMatchingBracket(const Buffer& buf, int line, int col) const {
    if (line < 0 || line >= buf.lineCount()) return {};
    std::string ln = buf.getLine(line);
    if (col < 0 || col >= static_cast<int>(ln.size())) return {};

    char ch = ln[col];

    if (isOpenBracket(ch)) {
        return scanForBracket(buf, line, col, openToClose_.at(ch), ch, ScanDir::Forward);
    }
    if (isCloseBracket(ch)) {
        return scanForBracket(buf, line, col, closeToOpen_.at(ch), ch, ScanDir::Backward);
    }

    return {};
}

// Add an extra word character
// Ekstra kelime karakteri ekle
void CharClassifier::addWordChar(char c) {
    extraWordChars_.insert(c);
}

// Remove an extra word character
// Ekstra kelime karakterini kaldir
void CharClassifier::removeWordChar(char c) {
    extraWordChars_.erase(c);
}

// Add a custom bracket pair
// Ozel parantez cifti ekle
void CharClassifier::addBracketPair(char open, char close) {
    brackets_.push_back({open, close});
    openToClose_[open] = close;
    closeToOpen_[close] = open;
}

// Get all bracket pairs
// Tum parantez ciftlerini al
const std::vector<BracketPair>& CharClassifier::bracketPairs() const {
    return brackets_;
}
