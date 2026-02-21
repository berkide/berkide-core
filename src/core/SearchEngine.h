#pragma once
#include <string>
#include <vector>
#include <regex>

class Buffer;
struct TextSpan;

// A single search match with position information
// Konum bilgisi iceren tek bir arama eslemesi
struct SearchMatch {
    int line;                    // Match line number / Esleme satir numarasi
    int col;                     // Match start column / Esleme baslangic sutunu
    int endCol;                  // Match end column / Esleme bitis sutunu
    int length;                  // Match length in characters / Esleme karakter uzunlugu
};

// Search configuration options
// Arama yapilandirma secenekleri
struct SearchOptions {
    bool caseSensitive = true;   // Case-sensitive search / Buyuk/kucuk harf duyarli arama
    bool regex         = false;  // Use regex pattern / Regex kalip kullan
    bool wholeWord     = false;  // Match whole words only / Yalnizca tam sozcukleri esle
    bool wrapAround    = true;   // Wrap around buffer boundaries / Buffer sinirlarinda sar
};

// Core search engine for find, find-next, replace operations in a buffer.
// Buffer icinde bul, sonrakini-bul, degistir islemleri icin temel arama motoru.
// Supports literal and regex search, forward/backward direction,
// case sensitivity, whole word matching, and wrap-around.
// Literal ve regex arama, ileri/geri yon, buyuk/kucuk harf duyarliligi,
// tam sozcuk esleme ve sarma destekler.
class SearchEngine {
public:
    SearchEngine();

    // Find the first match starting from (line, col) going forward
    // (line, col) konumundan ileri giderek ilk eslemeyi bul
    bool findForward(const Buffer& buf, const std::string& pattern,
                     int fromLine, int fromCol,
                     SearchMatch& match, const SearchOptions& opts = {}) const;

    // Find the first match starting from (line, col) going backward
    // (line, col) konumundan geri giderek ilk eslemeyi bul
    bool findBackward(const Buffer& buf, const std::string& pattern,
                      int fromLine, int fromCol,
                      SearchMatch& match, const SearchOptions& opts = {}) const;

    // Find all matches in the entire buffer
    // Tum buffer'daki tum eslemeleri bul
    std::vector<SearchMatch> findAll(const Buffer& buf, const std::string& pattern,
                                     const SearchOptions& opts = {}) const;

    // Replace the first match at (line, col) with replacement text
    // (line, col) konumundaki ilk eslemeyi degistirme metniyle degistir
    // Returns true if a replacement was made
    // Degistirme yapildiysa true dondurur
    bool replaceNext(Buffer& buf, const std::string& pattern,
                     const std::string& replacement,
                     int fromLine, int fromCol,
                     SearchMatch& nextMatch, const SearchOptions& opts = {});

    // Replace all occurrences in the buffer
    // Buffer'daki tum oluslari degistir
    // Returns the number of replacements made
    // Yapilan degistirme sayisini dondurur
    int replaceAll(Buffer& buf, const std::string& pattern,
                   const std::string& replacement,
                   const SearchOptions& opts = {});

    // Get match count for a pattern (useful for status display)
    // Bir kalip icin esleme sayisini al (durum gosterimi icin kullanisli)
    int countMatches(const Buffer& buf, const std::string& pattern,
                     const SearchOptions& opts = {}) const;

    // Store last search state for find-next/find-prev
    // Sonrakini-bul/oncekini-bul icin son arama durumunu sakla
    void setLastPattern(const std::string& pattern) { lastPattern_ = pattern; }
    const std::string& lastPattern() const { return lastPattern_; }

    void setLastOptions(const SearchOptions& opts) { lastOpts_ = opts; }
    const SearchOptions& lastOptions() const { return lastOpts_; }

private:
    // Find a literal pattern in a single line starting from column
    // Bir satirda sutundan baslayarak literal kalip bul
    bool findInLine(const std::string& line, const std::string& pattern,
                    int startCol, bool caseSensitive, bool wholeWord,
                    int& matchCol, int& matchLen) const;

    // Find a regex pattern in a single line starting from column
    // Bir satirda sutundan baslayarak regex kalip bul
    bool findRegexInLine(const std::string& line, const std::regex& re,
                         int startCol, int& matchCol, int& matchLen) const;

    // Convert pattern to lowercase for case-insensitive comparison
    // Buyuk/kucuk harf duyarsiz karsilastirma icin kalibi kucuk harfe cevir
    static std::string toLower(const std::string& s);

    // Check if character at position is a word boundary
    // Konumdaki karakterin sozcuk siniri olup olmadigini kontrol et
    static bool isWordBoundary(const std::string& line, int pos);

    std::string lastPattern_;    // Last searched pattern / Son aranan kalip
    SearchOptions lastOpts_;     // Last search options / Son arama secenekleri
};
