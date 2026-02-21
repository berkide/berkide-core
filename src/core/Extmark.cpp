#include "Extmark.h"
#include <algorithm>

// Default constructor
// Varsayilan kurucu
ExtmarkManager::ExtmarkManager() = default;

// Add a new extmark at a text range
// Bir metin araliginda yeni bir extmark ekle
int ExtmarkManager::set(const std::string& ns, int startLine, int startCol,
                        int endLine, int endCol,
                        const std::string& type, const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = nextId_.fetch_add(1);
    marks_[id] = {id, startLine, startCol, endLine, endCol, ns, type, data};
    return id;
}

// Add a new extmark with virtual text at a text range
// Bir metin araliginda sanal metinli yeni bir extmark ekle
int ExtmarkManager::setWithVirtText(const std::string& ns, int startLine, int startCol,
                                     int endLine, int endCol,
                                     const std::string& virtText, VirtTextPos virtPos,
                                     const std::string& virtStyle,
                                     const std::string& type, const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = nextId_.fetch_add(1);
    Extmark m;
    m.id = id;
    m.startLine = startLine;
    m.startCol = startCol;
    m.endLine = endLine;
    m.endCol = endCol;
    m.ns = ns;
    m.type = type;
    m.data = data;
    m.virtText = virtText;
    m.virtTextPos = virtPos;
    m.virtTextStyle = virtStyle;
    marks_[id] = std::move(m);
    return id;
}

// Get extmark by ID
// Kimlige gore extmark al
const Extmark* ExtmarkManager::get(int id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = marks_.find(id);
    if (it != marks_.end()) return &it->second;
    return nullptr;
}

// Remove extmark by ID
// Kimlige gore extmark sil
bool ExtmarkManager::remove(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return marks_.erase(id) > 0;
}

// Clear all extmarks in a namespace
// Bir ad alanindaki tum extmark'lari temizle
int ExtmarkManager::clearNamespace(const std::string& ns) {
    std::lock_guard<std::mutex> lock(mutex_);
    int removed = 0;
    auto it = marks_.begin();
    while (it != marks_.end()) {
        if (it->second.ns == ns) {
            it = marks_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

// Get all extmarks overlapping a line range
// Bir satir araligini kesen tum extmark'lari al
std::vector<const Extmark*> ExtmarkManager::getInRange(int startLine, int endLine,
                                                        const std::string& ns) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<const Extmark*> result;
    for (auto& [id, m] : marks_) {
        if (!ns.empty() && m.ns != ns) continue;
        // Check overlap: mark range intersects query range
        // Kesisim kontrol: isaret araligi sorgu araligini kesiyor mu
        if (m.startLine <= endLine && m.endLine >= startLine) {
            result.push_back(&m);
        }
    }
    return result;
}

// Get all extmarks on a specific line
// Belirli bir satirdaki tum extmark'lari al
std::vector<const Extmark*> ExtmarkManager::getOnLine(int line, const std::string& ns) const {
    return getInRange(line, line, ns);
}

// Adjust extmark positions after text insertion
// Metin eklemesinden sonra extmark konumlarini ayarla
void ExtmarkManager::adjustForInsert(int line, int col, int linesAdded, int colsAdded) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, m] : marks_) {
        // Adjust start position
        // Baslangic konumunu ayarla
        if (m.startLine > line || (m.startLine == line && m.startCol >= col)) {
            if (linesAdded > 0) {
                m.startLine += linesAdded;
                if (m.startLine == line + linesAdded) {
                    m.startCol = m.startCol - col + colsAdded;
                }
            } else {
                m.startCol += colsAdded;
            }
        }

        // Adjust end position
        // Bitis konumunu ayarla
        if (m.endLine > line || (m.endLine == line && m.endCol >= col)) {
            if (linesAdded > 0) {
                m.endLine += linesAdded;
                if (m.endLine == line + linesAdded) {
                    m.endCol = m.endCol - col + colsAdded;
                }
            } else {
                m.endCol += colsAdded;
            }
        }
    }
}

// Adjust extmark positions after text deletion
// Metin silmesinden sonra extmark konumlarini ayarla
void ExtmarkManager::adjustForDelete(int startLine, int startCol, int endLine, int endCol) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = marks_.begin();
    while (it != marks_.end()) {
        auto& m = it->second;

        // If mark is entirely within deleted range, remove it
        // Isaret tamamen silinen araliktaysa kaldir
        if (m.startLine >= startLine && m.startCol >= startCol &&
            m.endLine <= endLine && m.endCol <= endCol) {
            it = marks_.erase(it);
            continue;
        }

        int linesDelta = endLine - startLine;

        // Adjust start position
        // Baslangic konumunu ayarla
        if (m.startLine > endLine) {
            m.startLine -= linesDelta;
        } else if (m.startLine == endLine && m.startCol >= endCol) {
            m.startLine = startLine;
            m.startCol = startCol + (m.startCol - endCol);
        } else if (m.startLine > startLine || (m.startLine == startLine && m.startCol > startCol)) {
            m.startLine = startLine;
            m.startCol = startCol;
        }

        // Adjust end position
        // Bitis konumunu ayarla
        if (m.endLine > endLine) {
            m.endLine -= linesDelta;
        } else if (m.endLine == endLine && m.endCol >= endCol) {
            m.endLine = startLine;
            m.endCol = startCol + (m.endCol - endCol);
        } else if (m.endLine > startLine || (m.endLine == startLine && m.endCol > startCol)) {
            m.endLine = startLine;
            m.endCol = startCol;
        }

        ++it;
    }
}

// List all extmarks, optionally filtered by namespace
// Tum extmark'lari listele, istege bagli olarak ad alanina gore filtrele
std::vector<const Extmark*> ExtmarkManager::list(const std::string& ns) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<const Extmark*> result;
    result.reserve(marks_.size());
    for (auto& [id, m] : marks_) {
        if (ns.empty() || m.ns == ns) {
            result.push_back(&m);
        }
    }
    return result;
}

// Get total count
// Toplam sayiyi al
size_t ExtmarkManager::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return marks_.size();
}

// Clear all extmarks
// Tum extmark'lari temizle
void ExtmarkManager::clearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    marks_.clear();
}
