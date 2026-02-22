// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "MarkManager.h"
#include "Logger.h"
#include <algorithm>
#include <cctype>

// Default constructor
// Varsayilan kurucu
MarkManager::MarkManager() {
    jumpList_.reserve(kMaxJumps);
    changeList_.reserve(kMaxChanges);
}

// Set a named mark at position
// Konumda adlandirilmis bir isaret ayarla
void MarkManager::set(const std::string& name, int line, int col,
                      const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    marks_[name] = {line, col};
    if (!filePath.empty()) {
        markFiles_[name] = filePath;
    }
    LOG_DEBUG("[Mark] Set '", name, "' at ", line, ":", col);
}

// Get a mark by name
// Isme gore isaret al
const Mark* MarkManager::get(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = marks_.find(name);
    if (it != marks_.end()) return &it->second;
    return nullptr;
}

// Get file path for a global mark
// Global isaret icin dosya yolunu al
std::string MarkManager::getFilePath(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = markFiles_.find(name);
    if (it != markFiles_.end()) return it->second;
    return "";
}

// Delete a named mark
// Adlandirilmis bir isareti sil
bool MarkManager::remove(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool removed = marks_.erase(name) > 0;
    markFiles_.erase(name);
    return removed;
}

// List all marks
// Tum isaretleri listele
std::vector<std::pair<std::string, Mark>> MarkManager::list() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<std::string, Mark>> result;
    result.reserve(marks_.size());
    for (auto& [name, mark] : marks_) {
        result.emplace_back(name, mark);
    }
    return result;
}

// Push current position to jump list
// Mevcut konumu atlama listesine it
void MarkManager::pushJump(const std::string& filePath, int line, int col) {
    std::lock_guard<std::mutex> lock(mutex_);

    // If we're not at the end of the jump list, truncate forward history
    // Atlama listesinin sonunda degilsek, ileri gecmisi kes
    if (jumpPos_ >= 0 && jumpPos_ < static_cast<int>(jumpList_.size()) - 1) {
        jumpList_.resize(static_cast<size_t>(jumpPos_ + 1));
    }

    jumpList_.push_back({filePath, line, col});

    // Trim if exceeds max size
    // Maksimum boyutu asarsa kes
    if (static_cast<int>(jumpList_.size()) > kMaxJumps) {
        jumpList_.erase(jumpList_.begin());
    }

    jumpPos_ = static_cast<int>(jumpList_.size()) - 1;
}

// Navigate backward in jump list
// Atlama listesinde geri git
bool MarkManager::jumpBack(JumpEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (jumpList_.empty() || jumpPos_ <= 0) return false;
    --jumpPos_;
    entry = jumpList_[static_cast<size_t>(jumpPos_)];
    return true;
}

// Navigate forward in jump list
// Atlama listesinde ileri git
bool MarkManager::jumpForward(JumpEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (jumpList_.empty() || jumpPos_ >= static_cast<int>(jumpList_.size()) - 1) return false;
    ++jumpPos_;
    entry = jumpList_[static_cast<size_t>(jumpPos_)];
    return true;
}

// Record an edit position in the change list
// Degisiklik listesinde bir duzenleme konumunu kaydet
void MarkManager::recordEdit(int line, int col) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Update the '.' auto-mark (last edit position)
    // '.' otomatik isaretini guncelle (son duzenleme konumu)
    marks_["."] = {line, col};

    // Add to change list (skip if same position as last entry)
    // Degisiklik listesine ekle (son girisle ayni konumdaysa atla)
    if (!changeList_.empty()) {
        auto& last = changeList_.back();
        if (last.line == line && last.col == col) return;
    }

    changeList_.push_back({"", line, col});
    if (static_cast<int>(changeList_.size()) > kMaxChanges) {
        changeList_.erase(changeList_.begin());
    }
    changePos_ = static_cast<int>(changeList_.size()) - 1;
}

// Navigate to previous edit position
// Onceki duzenleme konumuna git
bool MarkManager::prevChange(JumpEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (changeList_.empty() || changePos_ <= 0) return false;
    --changePos_;
    entry = changeList_[static_cast<size_t>(changePos_)];
    return true;
}

// Navigate to next edit position
// Sonraki duzenleme konumuna git
bool MarkManager::nextChange(JumpEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (changeList_.empty() || changePos_ >= static_cast<int>(changeList_.size()) - 1) return false;
    ++changePos_;
    entry = changeList_[static_cast<size_t>(changePos_)];
    return true;
}

// Adjust marks after text edit (shift positions when lines are inserted/deleted)
// Metin duzenlemesinden sonra isaretleri ayarla (satirlar eklendiginde/silindiginde konumlari kaydir)
void MarkManager::adjustMarks(int editLine, int editCol, int linesDelta, int colDelta) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [name, mark] : marks_) {
        if (mark.line > editLine) {
            mark.line += linesDelta;
        } else if (mark.line == editLine && mark.col >= editCol) {
            if (linesDelta != 0) {
                mark.line += linesDelta;
                mark.col -= editCol;
            } else {
                mark.col += colDelta;
            }
        }
        // Clamp to non-negative
        // Negatif olmamaya hizala
        if (mark.line < 0) mark.line = 0;
        if (mark.col < 0)  mark.col = 0;
    }
}

// Clear buffer-local marks (a-z and auto-marks)
// Buffer-yerel isaretleri temizle (a-z ve otomatik isaretler)
void MarkManager::clearLocal() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = marks_.begin();
    while (it != marks_.end()) {
        const std::string& name = it->first;
        // Keep global marks (A-Z), remove everything else
        // Global isaretleri tut (A-Z), geri kalan her seyi kaldir
        if (name.size() == 1 && std::isupper(static_cast<unsigned char>(name[0]))) {
            ++it;
        } else {
            markFiles_.erase(name);
            it = marks_.erase(it);
        }
    }
}

// Clear all marks including global
// Global dahil tum isaretleri temizle
void MarkManager::clearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    marks_.clear();
    markFiles_.clear();
    jumpList_.clear();
    jumpPos_ = -1;
    changeList_.clear();
    changePos_ = -1;
}
