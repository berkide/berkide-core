#pragma once
#include <string>
#include <vector>

// Type of change in a diff hunk
// Diff parcasindaki degisiklik turu
enum class DiffType {
    Equal,     // Lines are the same / Satirlar ayni
    Insert,    // Lines added in new text / Yeni metinde eklenen satirlar
    Delete,    // Lines removed from old text / Eski metinden silinen satirlar
    Replace    // Lines changed (delete + insert) / Degisen satirlar (sil + ekle)
};

// A single hunk in a diff result
// Diff sonucundaki tek bir parca
struct DiffHunk {
    DiffType type;
    int oldStart = 0;        // Start line in old text / Eski metinde baslangic satiri
    int oldCount = 0;        // Number of lines in old text / Eski metinde satir sayisi
    int newStart = 0;        // Start line in new text / Yeni metinde baslangic satiri
    int newCount = 0;        // Number of lines in new text / Yeni metinde satir sayisi
    std::vector<std::string> oldLines;  // Lines from old text / Eski metinden satirlar
    std::vector<std::string> newLines;  // Lines from new text / Yeni metinden satirlar
};

// Result of a three-way merge
// Uc yonlu birlestirme sonucu
struct MergeResult {
    std::vector<std::string> lines;     // Merged output lines / Birlestirilmis cikis satirlari
    bool hasConflicts = false;          // Whether merge had conflicts / Birlestirmede catisma olup olmadigi
    int conflictCount = 0;              // Number of conflicts / Catisma sayisi
};

// Diff engine using the Myers diff algorithm.
// Myers diff algoritmasi kullanan diff motoru.
// Computes line-based diffs between two texts and supports three-way merge.
// Iki metin arasinda satir bazli diff hesaplar ve uc yonlu birlestirmeyi destekler.
class DiffEngine {
public:
    DiffEngine();

    // Compute diff between two texts (line-based)
    // Iki metin arasinda diff hesapla (satir bazli)
    std::vector<DiffHunk> diff(const std::vector<std::string>& oldLines,
                                const std::vector<std::string>& newLines) const;

    // Compute diff between two text strings (split by newlines)
    // Iki metin dizesi arasinda diff hesapla (yeni satirlara gore bol)
    std::vector<DiffHunk> diffText(const std::string& oldText,
                                    const std::string& newText) const;

    // Generate unified diff format string
    // Birlesik diff formati dizesi olustur
    std::string unifiedDiff(const std::vector<DiffHunk>& hunks,
                             const std::string& oldName = "a",
                             const std::string& newName = "b",
                             int contextLines = 3) const;

    // Apply a list of hunks to text (patch)
    // Metin uzerine parca listesini uygula (yama)
    std::vector<std::string> applyPatch(const std::vector<std::string>& original,
                                         const std::vector<DiffHunk>& hunks) const;

    // Three-way merge: base + ours + theirs
    // Uc yonlu birlestirme: temel + bizim + onlarin
    MergeResult merge3(const std::vector<std::string>& base,
                        const std::vector<std::string>& ours,
                        const std::vector<std::string>& theirs) const;

    // Count changes in a diff
    // Diff'teki degisiklikleri say
    int countInsertions(const std::vector<DiffHunk>& hunks) const;
    int countDeletions(const std::vector<DiffHunk>& hunks) const;

private:
    // Myers algorithm: compute shortest edit script
    // Myers algoritmasi: en kisa duzenleme betigini hesapla
    struct Edit {
        DiffType type;
        int oldIdx;
        int newIdx;
    };
    std::vector<Edit> myersDiff(const std::vector<std::string>& a,
                                 const std::vector<std::string>& b) const;

    // Split text into lines
    // Metni satirlara bol
    static std::vector<std::string> splitLines(const std::string& text);

    // Group edits into hunks
    // Duzenlemeieri parcalara grupla
    std::vector<DiffHunk> editsToHunks(const std::vector<Edit>& edits,
                                        const std::vector<std::string>& oldLines,
                                        const std::vector<std::string>& newLines) const;
};
