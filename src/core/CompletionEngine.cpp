// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "CompletionEngine.h"
#include <algorithm>
#include <cctype>
#include <unordered_set>

// Default constructor
// Varsayilan kurucu
CompletionEngine::CompletionEngine() = default;

// Check if character at index is a word boundary
// Dizindeki karakterin kelime siniri olup olmadigini kontrol et
bool CompletionEngine::isWordBoundary(const std::string& text, int index) {
    if (index <= 0) return true;
    char prev = text[index - 1];
    char curr = text[index];

    // Underscore boundary: _ to non-underscore
    // Alt cizgi siniri: _ 'den alt cizgi olmayana
    if (prev == '_' && curr != '_') return true;

    // camelCase boundary: lowercase to uppercase
    // camelCase siniri: kucuk harften buyuk harfe
    if (std::islower(static_cast<unsigned char>(prev)) &&
        std::isupper(static_cast<unsigned char>(curr))) return true;

    // Separator characters
    // Ayirici karakterler
    if (!std::isalnum(static_cast<unsigned char>(prev)) &&
        std::isalnum(static_cast<unsigned char>(curr))) return true;

    return false;
}

// Core fuzzy scoring algorithm
// Cekirdek bulanik puanlama algoritmasi
double CompletionEngine::fuzzyScore(const std::string& text, const std::string& query,
                                      std::vector<int>* positions) const {
    if (query.empty()) return 1.0;
    if (text.empty()) return 0.0;

    int tLen = static_cast<int>(text.size());
    int qLen = static_cast<int>(query.size());

    // Quick check: query must fit in text
    // Hizli kontrol: sorgu metne sigmali
    if (qLen > tLen) return 0.0;

    // Try to match all query chars in order (case insensitive)
    // Tum sorgu karakterlerini sirayla eslemeye calis (buyuk/kucuk harf duyarsiz)
    std::vector<int> matchPos;
    matchPos.reserve(qLen);

    int ti = 0;
    for (int qi = 0; qi < qLen; ++qi) {
        char qc = static_cast<char>(std::tolower(static_cast<unsigned char>(query[qi])));
        bool found = false;
        while (ti < tLen) {
            char tc = static_cast<char>(std::tolower(static_cast<unsigned char>(text[ti])));
            if (tc == qc) {
                matchPos.push_back(ti);
                ++ti;
                found = true;
                break;
            }
            ++ti;
        }
        if (!found) return 0.0;  // Query char not found / Sorgu karakteri bulunamadi
    }

    // Calculate score based on match quality
    // Esleme kalitesine gore skor hesapla
    double score = 0.0;

    for (int i = 0; i < qLen; ++i) {
        int pos = matchPos[i];

        // Base score for matching
        // Esleme icin temel skor
        score += 1.0;

        // Exact case match bonus
        // Tam buyuk/kucuk harf esleme bonusu
        if (text[pos] == query[i]) {
            score += 0.5;
        }

        // Word boundary bonus (camelCase, underscore, start of string)
        // Kelime siniri bonusu (camelCase, alt cizgi, dize basi)
        if (isWordBoundary(text, pos)) {
            score += 2.0;
        }

        // First character bonus
        // Ilk karakter bonusu
        if (pos == 0) {
            score += 3.0;
        }

        // Consecutive match bonus
        // Ardisik esleme bonusu
        if (i > 0 && matchPos[i] == matchPos[i - 1] + 1) {
            score += 1.5;
        }

        // Penalty for gap between matches
        // Eslemeler arasindaki bosluk icin ceza
        if (i > 0) {
            int gap = matchPos[i] - matchPos[i - 1] - 1;
            score -= gap * 0.1;
        }
    }

    // Prefer shorter texts (closer match density)
    // Daha kisa metinleri tercih et (daha yakin esleme yogunlugu)
    score *= static_cast<double>(qLen) / static_cast<double>(tLen);

    // Normalize to positive range
    // Pozitif araliga normalize et
    if (score < 0) score = 0.001;

    if (positions) {
        *positions = std::move(matchPos);
    }

    return score;
}

// Score a single candidate
// Tek bir adayi puanla
double CompletionEngine::score(const std::string& candidate, const std::string& query,
                                std::vector<int>* matchPositions) const {
    return fuzzyScore(candidate, query, matchPositions);
}

// Filter and score candidates, return sorted results
// Adaylari filtrele ve puanla, siralanmis sonuclari dondur
std::vector<CompletionItem> CompletionEngine::filter(
    const std::vector<CompletionItem>& candidates,
    const std::string& query) const {

    std::vector<CompletionItem> results;

    for (auto& item : candidates) {
        // Score against the display text
        // Gosterim metnine karsi puanla
        const std::string& target = item.label.empty() ? item.text : item.label;
        std::vector<int> positions;
        double s = fuzzyScore(target, query, &positions);

        if (s > 0) {
            CompletionItem result = item;
            result.score = s;
            result.matchPositions = std::move(positions);
            results.push_back(std::move(result));
        }
    }

    // Sort by score descending
    // Skora gore azalan sirala
    std::sort(results.begin(), results.end(),
              [](const CompletionItem& a, const CompletionItem& b) {
                  return a.score > b.score;
              });

    // Limit results
    // Sonuclari sinirla
    if (maxResults_ > 0 && static_cast<int>(results.size()) > maxResults_) {
        results.resize(maxResults_);
    }

    return results;
}

// Sort items by score
// Ogeleri skora gore sirala
void CompletionEngine::sortByScore(std::vector<CompletionItem>& items) const {
    std::sort(items.begin(), items.end(),
              [](const CompletionItem& a, const CompletionItem& b) {
                  return a.score > b.score;
              });
}

// Extract unique words from text
// Metinden benzersiz kelimeleri cikar
std::vector<std::string> CompletionEngine::extractWords(const std::string& text) {
    std::unordered_set<std::string> seen;
    std::vector<std::string> words;
    std::string word;

    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            word += c;
        } else {
            if (word.size() >= 2 && seen.find(word) == seen.end()) {
                seen.insert(word);
                words.push_back(word);
            }
            word.clear();
        }
    }

    // Last word
    // Son kelime
    if (word.size() >= 2 && seen.find(word) == seen.end()) {
        words.push_back(word);
    }

    return words;
}
