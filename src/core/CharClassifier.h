// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

class Buffer;

// Character type classification
// Karakter tipi siniflandirmasi
enum class CharType { Whitespace, Word, Punctuation, LineBreak, Other };

// Bracket pair for matching
// Eslestirme icin parantez cifti
struct BracketPair { char open; char close; };

// Word boundary result
// Kelime siniri sonucu
struct WordRange {
    int startCol;
    int endCol;
    std::string text;
};

// Bracket match result
// Parantez eslestirme sonucu
struct BracketMatch {
    bool found = false;
    int line = -1;
    int col = -1;
    char bracket = 0;
};

// Classifies characters and provides word boundary, bracket matching operations.
// Karakterleri siniflandirir ve kelime siniri, parantez eslestirme islemleri saglar.
class CharClassifier {
public:
    CharClassifier();

    // Classify a single character
    // Tek bir karakteri siniflandir
    CharType classify(char c) const;

    // Character type checks
    // Karakter tipi kontrolleri
    bool isWord(char c) const;
    bool isWhitespace(char c) const;
    bool isBracket(char c) const;
    bool isOpenBracket(char c) const;
    bool isCloseBracket(char c) const;
    char matchingBracket(char c) const;

    // Find word boundaries at column position in a line
    // Bir satirdaki sutun konumunda kelime sinirlarini bul
    WordRange wordAt(const std::string& line, int col) const;

    // Word navigation positions
    // Kelime gezinme konumlari
    int nextWordStart(const std::string& line, int col) const;
    int prevWordStart(const std::string& line, int col) const;
    int wordEnd(const std::string& line, int col) const;

    // Find matching bracket in buffer from position
    // Konumdan buffer'da eslesen parantezi bul
    BracketMatch findMatchingBracket(const Buffer& buf, int line, int col) const;

    // Configuration: add/remove extra word characters per language
    // Yapilandirma: dil basina ekstra kelime karakterleri ekle/kaldir
    void addWordChar(char c);
    void removeWordChar(char c);
    void addBracketPair(char open, char close);
    const std::vector<BracketPair>& bracketPairs() const;

private:
    std::unordered_set<char> extraWordChars_;
    std::vector<BracketPair> brackets_;
    std::unordered_map<char, char> openToClose_;
    std::unordered_map<char, char> closeToOpen_;

    enum class ScanDir { Forward, Backward };
    BracketMatch scanForBracket(const Buffer& buf, int line, int col,
                                 char target, char self, ScanDir dir) const;
};
