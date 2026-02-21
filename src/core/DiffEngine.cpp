#include "DiffEngine.h"
#include <algorithm>
#include <sstream>
#include <unordered_map>

// Default constructor
// Varsayilan kurucu
DiffEngine::DiffEngine() = default;

// Split text into lines by newline
// Metni yeni satira gore satirlara bol
std::vector<std::string> DiffEngine::splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    // If text ends with newline, getline won't produce trailing empty
    // Metin yeni satirla bitiyorsa, getline sondaki bosu uretmez
    if (!text.empty() && text.back() == '\n') {
        lines.push_back("");
    }
    return lines;
}

// Myers diff algorithm - O((N+M)D) time and space
// Myers diff algoritmasi - O((N+M)D) zaman ve alan
std::vector<DiffEngine::Edit> DiffEngine::myersDiff(
    const std::vector<std::string>& a,
    const std::vector<std::string>& b) const {

    int n = static_cast<int>(a.size());
    int m = static_cast<int>(b.size());

    // Special cases
    // Ozel durumlar
    if (n == 0 && m == 0) return {};

    if (n == 0) {
        std::vector<Edit> edits;
        for (int j = 0; j < m; ++j) {
            edits.push_back({DiffType::Insert, 0, j});
        }
        return edits;
    }

    if (m == 0) {
        std::vector<Edit> edits;
        for (int i = 0; i < n; ++i) {
            edits.push_back({DiffType::Delete, i, 0});
        }
        return edits;
    }

    int maxD = n + m;
    // V array indexed from -maxD to maxD
    // V dizisi -maxD'den maxD'ye indekslenir
    std::vector<int> v(2 * maxD + 1, 0);
    auto vIdx = [maxD](int k) { return k + maxD; };

    // Store traces for backtracking
    // Geri izleme icin izleri sakla
    std::vector<std::vector<int>> traces;

    bool found = false;
    for (int d = 0; d <= maxD && !found; ++d) {
        traces.push_back(v);

        for (int k = -d; k <= d; k += 2) {
            int x;
            if (k == -d || (k != d && v[vIdx(k - 1)] < v[vIdx(k + 1)])) {
                x = v[vIdx(k + 1)];  // Move down / Asagi git
            } else {
                x = v[vIdx(k - 1)] + 1;  // Move right / Saga git
            }
            int y = x - k;

            // Follow diagonal (matching lines)
            // Capraz takip et (eslesen satirlar)
            while (x < n && y < m && a[x] == b[y]) {
                ++x;
                ++y;
            }

            v[vIdx(k)] = x;

            if (x >= n && y >= m) {
                found = true;
                break;
            }
        }
    }

    // Backtrack to find the edit sequence
    // Duzenleme sirasini bulmak icin geri izle
    std::vector<Edit> edits;
    int x = n, y = m;

    for (int d = static_cast<int>(traces.size()) - 1; d >= 0; --d) {
        auto& vd = traces[d];
        int k = x - y;

        int prevK;
        if (k == -d || (k != d && vd[vIdx(k - 1)] < vd[vIdx(k + 1)])) {
            prevK = k + 1;
        } else {
            prevK = k - 1;
        }

        int prevX = vd[vIdx(prevK)];
        int prevY = prevX - prevK;

        // Diagonal moves (equal lines)
        // Capraz hareketler (esit satirlar)
        while (x > prevX && y > prevY) {
            --x;
            --y;
            edits.push_back({DiffType::Equal, x, y});
        }

        if (d > 0) {
            if (x == prevX) {
                // Insert
                // Ekle
                --y;
                edits.push_back({DiffType::Insert, x, y});
            } else {
                // Delete
                // Sil
                --x;
                edits.push_back({DiffType::Delete, x, y});
            }
        }
    }

    std::reverse(edits.begin(), edits.end());
    return edits;
}

// Group raw edits into hunks
// Ham duzenlemeieri parcalara grupla
std::vector<DiffHunk> DiffEngine::editsToHunks(
    const std::vector<Edit>& edits,
    const std::vector<std::string>& oldLines,
    const std::vector<std::string>& newLines) const {

    std::vector<DiffHunk> hunks;
    size_t i = 0;

    while (i < edits.size()) {
        // Skip equal lines
        // Esit satirlari atla
        if (edits[i].type == DiffType::Equal) {
            ++i;
            continue;
        }

        DiffHunk hunk;
        hunk.oldStart = edits[i].oldIdx;
        hunk.newStart = edits[i].newIdx;

        // Collect consecutive non-equal edits
        // Ardisik esit olmayan duzenlemeieri topla
        while (i < edits.size() && edits[i].type != DiffType::Equal) {
            if (edits[i].type == DiffType::Delete) {
                hunk.oldLines.push_back(oldLines[edits[i].oldIdx]);
                hunk.oldCount++;
            } else if (edits[i].type == DiffType::Insert) {
                hunk.newLines.push_back(newLines[edits[i].newIdx]);
                hunk.newCount++;
            }
            ++i;
        }

        // Determine hunk type
        // Parca turunu belirle
        if (hunk.oldCount > 0 && hunk.newCount > 0) {
            hunk.type = DiffType::Replace;
        } else if (hunk.oldCount > 0) {
            hunk.type = DiffType::Delete;
        } else {
            hunk.type = DiffType::Insert;
        }

        hunks.push_back(std::move(hunk));
    }

    return hunks;
}

// Compute diff between two line arrays
// Iki satir dizisi arasinda diff hesapla
std::vector<DiffHunk> DiffEngine::diff(
    const std::vector<std::string>& oldLines,
    const std::vector<std::string>& newLines) const {

    auto edits = myersDiff(oldLines, newLines);
    return editsToHunks(edits, oldLines, newLines);
}

// Compute diff between two text strings
// Iki metin dizesi arasinda diff hesapla
std::vector<DiffHunk> DiffEngine::diffText(
    const std::string& oldText,
    const std::string& newText) const {

    auto oldLines = splitLines(oldText);
    auto newLines = splitLines(newText);
    return diff(oldLines, newLines);
}

// Generate unified diff format
// Birlesik diff formati olustur
std::string DiffEngine::unifiedDiff(
    const std::vector<DiffHunk>& hunks,
    const std::string& oldName,
    const std::string& newName,
    int contextLines) const {

    (void)contextLines;  // Context merging for future / Gelecek icin baglam birlestirme

    std::ostringstream out;
    out << "--- " << oldName << "\n";
    out << "+++ " << newName << "\n";

    for (auto& hunk : hunks) {
        out << "@@ -" << (hunk.oldStart + 1) << "," << hunk.oldCount
            << " +" << (hunk.newStart + 1) << "," << hunk.newCount << " @@\n";

        for (auto& line : hunk.oldLines) {
            out << "-" << line << "\n";
        }
        for (auto& line : hunk.newLines) {
            out << "+" << line << "\n";
        }
    }

    return out.str();
}

// Apply a patch to original text
// Orijinal metne yama uygula
std::vector<std::string> DiffEngine::applyPatch(
    const std::vector<std::string>& original,
    const std::vector<DiffHunk>& hunks) const {

    std::vector<std::string> result;
    int origIdx = 0;

    for (auto& hunk : hunks) {
        // Copy lines before this hunk
        // Bu parcadan onceki satirlari kopyala
        while (origIdx < hunk.oldStart) {
            result.push_back(original[origIdx]);
            ++origIdx;
        }

        // Add new lines from hunk
        // Parcadan yeni satirlar ekle
        for (auto& line : hunk.newLines) {
            result.push_back(line);
        }

        // Skip old lines that were deleted/replaced
        // Silinen/degistirilen eski satirlari atla
        origIdx += hunk.oldCount;
    }

    // Copy remaining lines
    // Kalan satirlari kopyala
    while (origIdx < static_cast<int>(original.size())) {
        result.push_back(original[origIdx]);
        ++origIdx;
    }

    return result;
}

// Three-way merge using diff3 approach
// diff3 yaklasimi kullanarak uc yonlu birlestirme
MergeResult DiffEngine::merge3(
    const std::vector<std::string>& base,
    const std::vector<std::string>& ours,
    const std::vector<std::string>& theirs) const {

    MergeResult result;
    auto editsOurs   = myersDiff(base, ours);
    auto editsTheirs = myersDiff(base, theirs);

    // Build line-level change maps: base line -> what happened
    // Satir duzeyinde degisiklik haritalari olustur: temel satir -> ne oldu
    std::unordered_map<int, bool> oursDeleted, theirsDeleted;
    std::unordered_map<int, std::vector<std::string>> oursInserted, theirsInserted;

    for (auto& e : editsOurs) {
        if (e.type == DiffType::Delete) oursDeleted[e.oldIdx] = true;
        if (e.type == DiffType::Insert) oursInserted[e.oldIdx].push_back(ours[e.newIdx]);
    }
    for (auto& e : editsTheirs) {
        if (e.type == DiffType::Delete) theirsDeleted[e.oldIdx] = true;
        if (e.type == DiffType::Insert) theirsInserted[e.oldIdx].push_back(theirs[e.newIdx]);
    }

    for (int i = 0; i < static_cast<int>(base.size()); ++i) {
        bool od = oursDeleted.count(i) > 0;
        bool td = theirsDeleted.count(i) > 0;
        bool oi = oursInserted.count(i) > 0;
        bool ti = theirsInserted.count(i) > 0;

        // Handle insertions before this line
        // Bu satirdan onceki eklemeleri isle
        if (oi && ti) {
            // Both inserted: check if same
            // Ikisi de ekledi: ayni mi kontrol et
            if (oursInserted[i] == theirsInserted[i]) {
                for (auto& l : oursInserted[i]) result.lines.push_back(l);
            } else {
                // Conflict
                // Catisma
                result.hasConflicts = true;
                result.conflictCount++;
                result.lines.push_back("<<<<<<< ours");
                for (auto& l : oursInserted[i]) result.lines.push_back(l);
                result.lines.push_back("=======");
                for (auto& l : theirsInserted[i]) result.lines.push_back(l);
                result.lines.push_back(">>>>>>> theirs");
            }
        } else if (oi) {
            for (auto& l : oursInserted[i]) result.lines.push_back(l);
        } else if (ti) {
            for (auto& l : theirsInserted[i]) result.lines.push_back(l);
        }

        // Handle the base line itself
        // Temel satirin kendisini isle
        if (od && td) {
            // Both deleted: skip
            // Ikisi de sildi: atla
        } else if (od) {
            // We deleted, they didn't: skip
            // Biz sildik, onlar silmedi: atla
        } else if (td) {
            // They deleted, we didn't: skip
            // Onlar sildi, biz silmedik: atla
        } else {
            result.lines.push_back(base[i]);
        }
    }

    // Handle trailing insertions after last base line
    // Son temel satirdan sonraki eklemeleri isle
    int lastIdx = static_cast<int>(base.size());
    bool oi = oursInserted.count(lastIdx) > 0;
    bool ti = theirsInserted.count(lastIdx) > 0;
    if (oi && ti) {
        if (oursInserted[lastIdx] == theirsInserted[lastIdx]) {
            for (auto& l : oursInserted[lastIdx]) result.lines.push_back(l);
        } else {
            result.hasConflicts = true;
            result.conflictCount++;
            result.lines.push_back("<<<<<<< ours");
            for (auto& l : oursInserted[lastIdx]) result.lines.push_back(l);
            result.lines.push_back("=======");
            for (auto& l : theirsInserted[lastIdx]) result.lines.push_back(l);
            result.lines.push_back(">>>>>>> theirs");
        }
    } else if (oi) {
        for (auto& l : oursInserted[lastIdx]) result.lines.push_back(l);
    } else if (ti) {
        for (auto& l : theirsInserted[lastIdx]) result.lines.push_back(l);
    }

    return result;
}

// Count insertions across all hunks
// Tum parcalardaki eklemeleri say
int DiffEngine::countInsertions(const std::vector<DiffHunk>& hunks) const {
    int count = 0;
    for (auto& h : hunks) count += h.newCount;
    return count;
}

// Count deletions across all hunks
// Tum parcalardaki silmeleri say
int DiffEngine::countDeletions(const std::vector<DiffHunk>& hunks) const {
    int count = 0;
    for (auto& h : hunks) count += h.oldCount;
    return count;
}
