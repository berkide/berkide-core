#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>

// A single fold region in a buffer
// Buffer'daki tek bir katlama bolgesi
struct Fold {
    int startLine;               // First line of fold / Katlamanin ilk satiri
    int endLine;                 // Last line of fold / Katlamanin son satiri
    bool collapsed = false;      // Whether the fold is collapsed / Katlamanin kapali olup olmadigi
    std::string label;           // Display label when collapsed (e.g., "...3 lines") / Kapandiginda gosterilen etiket
};

// Manages code folding regions in a buffer.
// Bir buffer'daki kod katlama bolgelerini yonetir.
// Supports manual folds (set by user/plugin) and provides integration points
// for tree-sitter/indent-based folding via plugins.
// El ile katlamalar (kullanici/eklenti tarafindan ayarlanan) destekler ve
// eklentiler araciligiyla tree-sitter/girinti tabanli katlama icin entegrasyon noktalari saglar.
class FoldManager {
public:
    FoldManager();

    // Create a fold region
    // Bir katlama bolgesi olustur
    void create(int startLine, int endLine, const std::string& label = "");

    // Remove a fold that starts at a given line
    // Verilen satirda baslayan bir katlamayi kaldir
    bool remove(int startLine);

    // Toggle fold at a given line (collapse/expand)
    // Verilen satirda katlamayi degistir (kapat/ac)
    bool toggle(int line);

    // Collapse a fold at a given line
    // Verilen satirda katlamayi kapat
    bool collapse(int line);

    // Expand a fold at a given line
    // Verilen satirda katlamayi ac
    bool expand(int line);

    // Collapse all folds
    // Tum katlamalari kapat
    void collapseAll();

    // Expand all folds
    // Tum katlamalari ac
    void expandAll();

    // Get the fold at a given line (line is within fold range)
    // Verilen satirdaki katlamayi al (satir katlama araliginda)
    const Fold* getFoldAt(int line) const;

    // Check if a line is hidden by a collapsed fold
    // Bir satirin kapali bir katlama tarafindan gizlenip gizlenmedigini kontrol et
    bool isLineHidden(int line) const;

    // Get all folds
    // Tum katlamalari al
    std::vector<Fold> list() const;

    // Get visible line count (total - hidden by collapsed folds)
    // Gorunen satir sayisini al (toplam - kapali katlamalar tarafindan gizlenen)
    int visibleLineCount(int totalLines) const;

    // Adjust folds after text edit (line insertion/deletion)
    // Metin duzenlemesinden sonra katlamalari ayarla (satir ekleme/silme)
    void adjustForInsert(int atLine, int linesAdded);
    void adjustForDelete(int startLine, int linesDeleted);

    // Clear all folds
    // Tum katlamalari temizle
    void clearAll();

private:
    mutable std::mutex mutex_;
    std::map<int, Fold> folds_;  // startLine -> Fold / baslangicSatiri -> Katlama
};
