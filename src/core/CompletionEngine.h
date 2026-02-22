// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include <vector>
#include <mutex>

// A single completion candidate with score
// Skorlu tek bir tamamlama adayi
struct CompletionItem {
    std::string text;            // The completion text / Tamamlama metni
    std::string label;           // Display label / Gosterim etiketi
    std::string detail;          // Additional detail / Ek detay
    std::string kind;            // Kind: "function", "variable", "keyword", etc. / Tur
    std::string insertText;      // Text to insert (may differ from label) / Eklenecek metin
    double score = 0.0;          // Match score (higher = better match) / Esleme skoru
    std::vector<int> matchPositions;  // Character positions that matched / Eslesen karakter konumlari
};

// Fuzzy matching and scoring engine for completion candidates.
// Tamamlama adaylari icin bulanik esleme ve puanlama motoru.
// Provides fast filtering and ranking of candidates against a query string.
// Bir sorgu dizesine karsi adaylarin hizli filtreleme ve siralamasini saglar.
class CompletionEngine {
public:
    CompletionEngine();

    // Filter and score candidates against a query (fuzzy match)
    // Adaylari bir sorguya karsi filtrele ve puanla (bulanik esleme)
    std::vector<CompletionItem> filter(const std::vector<CompletionItem>& candidates,
                                        const std::string& query) const;

    // Score a single candidate against a query (returns 0 if no match)
    // Tek bir adayi bir sorguya karsi puanla (esleme yoksa 0 dondurur)
    double score(const std::string& candidate, const std::string& query,
                  std::vector<int>* matchPositions = nullptr) const;

    // Sort candidates by score (descending)
    // Adaylari skora gore sirala (azalan)
    void sortByScore(std::vector<CompletionItem>& items) const;

    // Extract words from buffer text for word completion
    // Kelime tamamlama icin buffer metninden kelimeler cikar
    static std::vector<std::string> extractWords(const std::string& text);

    // Set maximum number of results to return
    // Dondurilecek maksimum sonuc sayisini ayarla
    void setMaxResults(int max) { maxResults_ = max; }
    int maxResults() const { return maxResults_; }

private:
    int maxResults_ = 50;

    // Fuzzy match scoring with consecutive bonus, camelCase bonus, etc.
    // Ardisik bonus, camelCase bonus vb. ile bulanik esleme puanlama
    double fuzzyScore(const std::string& text, const std::string& query,
                       std::vector<int>* positions) const;

    // Check if character is a word boundary (camelCase, underscore, etc.)
    // Karakterin kelime siniri olup olmadigini kontrol et (camelCase, alt cizgi, vb.)
    static bool isWordBoundary(const std::string& text, int index);
};
