// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "SearchEngine.h"
#include "buffer.h"
#include "Logger.h"
#include <algorithm>
#include <cctype>

// Default constructor
// Varsayilan kurucu
SearchEngine::SearchEngine() = default;

// Convert string to lowercase (for case-insensitive search)
// Dizeyi kucuk harfe cevir (buyuk/kucuk harf duyarsiz arama icin)
std::string SearchEngine::toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// Check if position is at a word boundary (start or end of a word)
// Konumun sozcuk sinirinda olup olmadigini kontrol et (sozcuk basi veya sonu)
bool SearchEngine::isWordBoundary(const std::string& line, int pos) {
    if (pos <= 0 || pos >= static_cast<int>(line.size())) return true;
    bool prevAlnum = std::isalnum(static_cast<unsigned char>(line[pos - 1])) || line[pos - 1] == '_';
    bool currAlnum = std::isalnum(static_cast<unsigned char>(line[pos])) || line[pos] == '_';
    return prevAlnum != currAlnum;
}

// Find a literal pattern in a single line
// Bir satirda literal kalip bul
bool SearchEngine::findInLine(const std::string& line, const std::string& pattern,
                              int startCol, bool caseSensitive, bool wholeWord,
                              int& matchCol, int& matchLen) const {
    if (pattern.empty()) return false;

    std::string haystack = caseSensitive ? line : toLower(line);
    std::string needle   = caseSensitive ? pattern : toLower(pattern);

    size_t pos = haystack.find(needle, static_cast<size_t>(startCol));
    while (pos != std::string::npos) {
        int mCol = static_cast<int>(pos);
        int mEnd = mCol + static_cast<int>(needle.size());

        if (!wholeWord || (isWordBoundary(line, mCol) && isWordBoundary(line, mEnd))) {
            matchCol = mCol;
            matchLen = static_cast<int>(needle.size());
            return true;
        }

        pos = haystack.find(needle, pos + 1);
    }
    return false;
}

// Find a regex pattern in a single line
// Bir satirda regex kalip bul
bool SearchEngine::findRegexInLine(const std::string& line, const std::regex& re,
                                   int startCol, int& matchCol, int& matchLen) const {
    std::string searchStr = line.substr(static_cast<size_t>(startCol));
    std::smatch m;
    if (std::regex_search(searchStr, m, re)) {
        matchCol = startCol + static_cast<int>(m.position(0));
        matchLen = static_cast<int>(m.length(0));
        return true;
    }
    return false;
}

// Find forward from (fromLine, fromCol), optionally wrapping around
// (fromLine, fromCol) konumundan ileri ara, istege bagli olarak sar
bool SearchEngine::findForward(const Buffer& buf, const std::string& pattern,
                               int fromLine, int fromCol,
                               SearchMatch& match, const SearchOptions& opts) const {
    if (pattern.empty() || buf.lineCount() == 0) return false;

    int totalLines = buf.lineCount();

    // Compile regex once if needed
    // Gerekirse regex'i bir kere derle
    std::regex re;
    if (opts.regex) {
        try {
            auto flags = std::regex_constants::ECMAScript;
            if (!opts.caseSensitive) flags |= std::regex_constants::icase;
            re = std::regex(pattern, flags);
        } catch (const std::regex_error& e) {
            LOG_WARN("[Search] Invalid regex: ", e.what());
            return false;
        }
    }

    // Search from current position to end of buffer
    // Mevcut konumdan buffer sonuna kadar ara
    for (int i = fromLine; i < totalLines; ++i) {
        std::string line = buf.getLine(i);
        int startCol = (i == fromLine) ? fromCol : 0;
        int mCol, mLen;

        bool found = opts.regex
            ? findRegexInLine(line, re, startCol, mCol, mLen)
            : findInLine(line, pattern, startCol, opts.caseSensitive, opts.wholeWord, mCol, mLen);

        if (found) {
            match = {i, mCol, mCol + mLen, mLen};
            return true;
        }
    }

    // Wrap around: search from beginning to current position
    // Sarma: baslangictan mevcut konuma kadar ara
    if (opts.wrapAround) {
        for (int i = 0; i <= fromLine && i < totalLines; ++i) {
            std::string line = buf.getLine(i);
            int startCol = 0;
            int endLimit = (i == fromLine) ? fromCol : static_cast<int>(line.size());
            int mCol, mLen;

            bool found = opts.regex
                ? findRegexInLine(line, re, startCol, mCol, mLen)
                : findInLine(line, pattern, startCol, opts.caseSensitive, opts.wholeWord, mCol, mLen);

            if (found && mCol < endLimit) {
                match = {i, mCol, mCol + mLen, mLen};
                return true;
            }
        }
    }

    return false;
}

// Find backward from (fromLine, fromCol), optionally wrapping around
// (fromLine, fromCol) konumundan geri ara, istege bagli olarak sar
bool SearchEngine::findBackward(const Buffer& buf, const std::string& pattern,
                                int fromLine, int fromCol,
                                SearchMatch& match, const SearchOptions& opts) const {
    if (pattern.empty() || buf.lineCount() == 0) return false;

    int totalLines = buf.lineCount();

    std::regex re;
    if (opts.regex) {
        try {
            auto flags = std::regex_constants::ECMAScript;
            if (!opts.caseSensitive) flags |= std::regex_constants::icase;
            re = std::regex(pattern, flags);
        } catch (const std::regex_error& e) {
            LOG_WARN("[Search] Invalid regex: ", e.what());
            return false;
        }
    }

    // Search backward: scan each line for the last match before fromCol
    // Geri arama: her satirda fromCol'dan onceki son eslemeyi tara
    for (int i = fromLine; i >= 0; --i) {
        std::string line = buf.getLine(i);
        int maxCol = (i == fromLine) ? fromCol : static_cast<int>(line.size());

        // Find all matches in line, take the last one before maxCol
        // Satirdaki tum eslemeleri bul, maxCol'dan onceki son birini al
        int lastMCol = -1, lastMLen = 0;
        int searchFrom = 0;

        while (true) {
            int mCol, mLen;
            bool found = opts.regex
                ? findRegexInLine(line, re, searchFrom, mCol, mLen)
                : findInLine(line, pattern, searchFrom, opts.caseSensitive, opts.wholeWord, mCol, mLen);

            if (!found || mCol >= maxCol) break;
            lastMCol = mCol;
            lastMLen = mLen;
            searchFrom = mCol + 1;
        }

        if (lastMCol >= 0) {
            match = {i, lastMCol, lastMCol + lastMLen, lastMLen};
            return true;
        }
    }

    // Wrap around: search from end to current position
    // Sarma: sondan mevcut konuma kadar ara
    if (opts.wrapAround) {
        for (int i = totalLines - 1; i >= fromLine; --i) {
            std::string line = buf.getLine(i);
            int maxCol = (i == fromLine) ? static_cast<int>(line.size()) : static_cast<int>(line.size());

            int lastMCol = -1, lastMLen = 0;
            int searchFrom = (i == fromLine) ? fromCol : 0;

            while (true) {
                int mCol, mLen;
                bool found = opts.regex
                    ? findRegexInLine(line, re, searchFrom, mCol, mLen)
                    : findInLine(line, pattern, searchFrom, opts.caseSensitive, opts.wholeWord, mCol, mLen);

                if (!found || mCol >= maxCol) break;
                lastMCol = mCol;
                lastMLen = mLen;
                searchFrom = mCol + 1;
            }

            if (lastMCol >= 0) {
                match = {i, lastMCol, lastMCol + lastMLen, lastMLen};
                return true;
            }
        }
    }

    return false;
}

// Find all matches in the entire buffer
// Tum buffer'daki tum eslemeleri bul
std::vector<SearchMatch> SearchEngine::findAll(const Buffer& buf, const std::string& pattern,
                                               const SearchOptions& opts) const {
    std::vector<SearchMatch> results;
    if (pattern.empty() || buf.lineCount() == 0) return results;

    std::regex re;
    if (opts.regex) {
        try {
            auto flags = std::regex_constants::ECMAScript;
            if (!opts.caseSensitive) flags |= std::regex_constants::icase;
            re = std::regex(pattern, flags);
        } catch (const std::regex_error&) {
            return results;
        }
    }

    for (int i = 0; i < buf.lineCount(); ++i) {
        std::string line = buf.getLine(i);
        int searchFrom = 0;

        while (true) {
            int mCol, mLen;
            bool found = opts.regex
                ? findRegexInLine(line, re, searchFrom, mCol, mLen)
                : findInLine(line, pattern, searchFrom, opts.caseSensitive, opts.wholeWord, mCol, mLen);

            if (!found) break;
            results.push_back({i, mCol, mCol + mLen, mLen});
            searchFrom = mCol + std::max(1, mLen); // Advance past match / Eslemeyi gec
        }
    }

    return results;
}

// Replace the first match at/after (fromLine, fromCol) and return the next match
// (fromLine, fromCol) konumundaki/sonrasindaki ilk eslemeyi degistir ve sonraki eslemeyi dondur
bool SearchEngine::replaceNext(Buffer& buf, const std::string& pattern,
                               const std::string& replacement,
                               int fromLine, int fromCol,
                               SearchMatch& nextMatch, const SearchOptions& opts) {
    SearchMatch current;
    if (!findForward(buf, pattern, fromLine, fromCol, current, opts)) return false;

    // Perform the replacement
    // Degistirmeyi gerceklestir
    std::string line = buf.getLine(current.line);
    std::string newContent;

    if (opts.regex) {
        // Regex replacement with backreference support ($1, $2, etc.)
        // Geri referans destekli regex degistirme ($1, $2, vb.)
        try {
            auto flags = std::regex_constants::ECMAScript;
            if (!opts.caseSensitive) flags |= std::regex_constants::icase;
            std::regex re(pattern, flags);
            std::string matchedPart = line.substr(current.col, current.length);
            newContent = std::regex_replace(matchedPart, re, replacement);
        } catch (const std::regex_error&) {
            newContent = replacement;
        }
    } else {
        newContent = replacement;
    }

    // Delete old text and insert new
    // Eski metni sil ve yenisini ekle
    buf.deleteRange(current.line, current.col,
                    current.line, current.col + current.length);
    buf.insertText(current.line, current.col, newContent);

    // Find the next match after the replacement
    // Degistirmeden sonra bir sonraki eslemeyi bul
    int nextCol = current.col + static_cast<int>(newContent.size());
    findForward(buf, pattern, current.line, nextCol, nextMatch, opts);

    return true;
}

// Replace all occurrences in the buffer (processes in reverse to preserve positions)
// Buffer'daki tum oluslari degistir (konumlari korumak icin ters sirada isler)
int SearchEngine::replaceAll(Buffer& buf, const std::string& pattern,
                             const std::string& replacement,
                             const SearchOptions& opts) {
    auto matches = findAll(buf, pattern, opts);
    if (matches.empty()) return 0;

    std::regex re;
    if (opts.regex) {
        try {
            auto flags = std::regex_constants::ECMAScript;
            if (!opts.caseSensitive) flags |= std::regex_constants::icase;
            re = std::regex(pattern, flags);
        } catch (const std::regex_error&) {
            return 0;
        }
    }

    // Process matches in reverse order to maintain valid positions
    // Gecerli konumlari korumak icin eslemeleri ters sirada isle
    int count = 0;
    for (int i = static_cast<int>(matches.size()) - 1; i >= 0; --i) {
        auto& m = matches[i];
        std::string newContent;

        if (opts.regex) {
            std::string matchedPart = buf.getLine(m.line).substr(m.col, m.length);
            newContent = std::regex_replace(matchedPart, re, replacement);
        } else {
            newContent = replacement;
        }

        buf.deleteRange(m.line, m.col, m.line, m.col + m.length);
        buf.insertText(m.line, m.col, newContent);
        ++count;
    }

    return count;
}

// Count total matches for a pattern
// Bir kalip icin toplam esleme sayisini say
int SearchEngine::countMatches(const Buffer& buf, const std::string& pattern,
                               const SearchOptions& opts) const {
    return static_cast<int>(findAll(buf, pattern, opts).size());
}
