#include "FoldManager.h"
#include <algorithm>

// Default constructor
// Varsayilan kurucu
FoldManager::FoldManager() = default;

// Create a fold region
// Bir katlama bolgesi olustur
void FoldManager::create(int startLine, int endLine, const std::string& label) {
    if (startLine >= endLine) return;
    std::lock_guard<std::mutex> lock(mutex_);

    std::string lbl = label;
    if (lbl.empty()) {
        lbl = "..." + std::to_string(endLine - startLine) + " lines";
    }
    folds_[startLine] = {startLine, endLine, false, lbl};
}

// Remove a fold starting at a given line
// Verilen satirda baslayan katlamayi kaldir
bool FoldManager::remove(int startLine) {
    std::lock_guard<std::mutex> lock(mutex_);
    return folds_.erase(startLine) > 0;
}

// Toggle fold at a line
// Bir satirdaki katlamayi degistir
bool FoldManager::toggle(int line) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [start, fold] : folds_) {
        if (line >= fold.startLine && line <= fold.endLine) {
            fold.collapsed = !fold.collapsed;
            return true;
        }
    }
    return false;
}

// Collapse fold at a line
// Bir satirdaki katlamayi kapat
bool FoldManager::collapse(int line) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [start, fold] : folds_) {
        if (line >= fold.startLine && line <= fold.endLine) {
            fold.collapsed = true;
            return true;
        }
    }
    return false;
}

// Expand fold at a line
// Bir satirdaki katlamayi ac
bool FoldManager::expand(int line) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [start, fold] : folds_) {
        if (line >= fold.startLine && line <= fold.endLine) {
            fold.collapsed = false;
            return true;
        }
    }
    return false;
}

// Collapse all folds
// Tum katlamalari kapat
void FoldManager::collapseAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [start, fold] : folds_) {
        fold.collapsed = true;
    }
}

// Expand all folds
// Tum katlamalari ac
void FoldManager::expandAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [start, fold] : folds_) {
        fold.collapsed = false;
    }
}

// Get fold at a specific line (checks if line falls within any fold range)
// Belirli bir satirdaki katlamayi al (satirin herhangi bir katlama araliginda olup olmadigini kontrol eder)
const Fold* FoldManager::getFoldAt(int line) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [start, fold] : folds_) {
        if (line >= fold.startLine && line <= fold.endLine) {
            return &fold;
        }
    }
    return nullptr;
}

// Check if a line is hidden by a collapsed fold
// Bir satirin kapali bir katlama tarafindan gizlenip gizlenmedigini kontrol et
bool FoldManager::isLineHidden(int line) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [start, fold] : folds_) {
        if (fold.collapsed && line > fold.startLine && line <= fold.endLine) {
            return true;
        }
    }
    return false;
}

// List all folds
// Tum katlamalari listele
std::vector<Fold> FoldManager::list() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Fold> result;
    result.reserve(folds_.size());
    for (auto& [start, fold] : folds_) {
        result.push_back(fold);
    }
    return result;
}

// Get visible line count (total minus hidden lines)
// Gorunen satir sayisini al (toplam eksi gizli satirlar)
int FoldManager::visibleLineCount(int totalLines) const {
    std::lock_guard<std::mutex> lock(mutex_);
    int hidden = 0;
    for (auto& [start, fold] : folds_) {
        if (fold.collapsed) {
            hidden += fold.endLine - fold.startLine; // Hide lines after startLine / Baslangic satirindan sonraki satirlari gizle
        }
    }
    return totalLines - hidden;
}

// Adjust folds after line insertion
// Satir eklemesinden sonra katlamalari ayarla
void FoldManager::adjustForInsert(int atLine, int linesAdded) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<int, Fold> adjusted;
    for (auto& [start, fold] : folds_) {
        if (fold.startLine >= atLine) {
            fold.startLine += linesAdded;
            fold.endLine += linesAdded;
        } else if (fold.endLine >= atLine) {
            fold.endLine += linesAdded;
        }
        adjusted[fold.startLine] = fold;
    }
    folds_ = std::move(adjusted);
}

// Adjust folds after line deletion
// Satir silmesinden sonra katlamalari ayarla
void FoldManager::adjustForDelete(int startLine, int linesDeleted) {
    std::lock_guard<std::mutex> lock(mutex_);
    int endLine = startLine + linesDeleted;
    std::map<int, Fold> adjusted;

    for (auto& [start, fold] : folds_) {
        // Fold entirely within deleted range: remove it
        // Katlama tamamen silinen araliktaysa: kaldir
        if (fold.startLine >= startLine && fold.endLine < endLine) continue;

        if (fold.startLine >= endLine) {
            // Fold entirely after deleted range: shift up
            // Katlama tamamen silinen araliktan sonra: yukari kaydir
            fold.startLine -= linesDeleted;
            fold.endLine -= linesDeleted;
        } else if (fold.endLine >= endLine) {
            // Fold partially overlaps deleted range: shrink
            // Katlama silinen aralikla kismi olarak ortusuyor: kucult
            fold.endLine -= linesDeleted;
            if (fold.endLine <= fold.startLine) continue;
        }
        adjusted[fold.startLine] = fold;
    }
    folds_ = std::move(adjusted);
}

// Clear all folds
// Tum katlamalari temizle
void FoldManager::clearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    folds_.clear();
}
