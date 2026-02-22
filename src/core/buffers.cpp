// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "buffers.h"
#include <filesystem>

// Constructor: initialize with one empty untitled document
// Kurucu: bir bos isimsiz belgeyle baslat
Buffers::Buffers() {
    newDocument();
}

// Create a new empty document and make it the active buffer
// Yeni bir bos belge olustur ve aktif tampon yap
size_t Buffers::newDocument(const std::string& untitledName) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto st = std::make_unique<EditorState>();
    st->setFilePath(untitledName);
    st->markModified(false);
    docs_.push_back(std::move(st));
    active_ = docs_.size() - 1;
    return active_;
}

// Open a file from disk; if already open, switch to it instead
// Diskten bir dosya ac; zaten aciksa ona gec
bool Buffers::openFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Check if already open
    for (size_t i = 0; i < docs_.size(); ++i) {
        if (docs_[i]->getFilePath() == path) {
            active_ = i;
            docs_[active_]->syncCursor();
            return true;
        }
    }

    auto st = std::make_unique<EditorState>();
    auto res = FileSystem::loadToBuffer(st->getBuffer(), path);
    if (!res.success) return false;

    st->setFilePath(path);
    st->markModified(false);
    docs_.push_back(std::move(st));
    active_ = docs_.size() - 1;
    return true;
}

// Save the currently active document to its file path
// Su an aktif olan belgeyi dosya yoluna kaydet
bool Buffers::saveActive() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (docs_.empty()) return false;
    auto& st = *docs_[active_];
    const auto& p = st.getFilePath();
    if (p.empty() || p == "untitled") return false;
    auto res = FileSystem::saveFromBuffer(st.getBuffer(), p);
    if (res.success) st.markModified(false);
    return res.success;
}

// Save all open documents that have a valid file path
// Gecerli dosya yolu olan tum acik belgeleri kaydet
int Buffers::saveAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    int ok = 0;
    for (auto& stPtr : docs_) {
        auto& st = *stPtr;
        const auto& p = st.getFilePath();
        if (!p.empty() && p != "untitled") {
            auto res = FileSystem::saveFromBuffer(st.getBuffer(), p);
            if (res.success) {
                st.markModified(false);
                ++ok;
            }
        }
    }
    return ok;
}

// Close the currently active document; create a new one if none remain
// Aktif belgeyi kapat; hicbiri kalmazsa yeni bir tane olustur
bool Buffers::closeActive() {
    bool needNew = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (docs_.empty()) return false;
        docs_.erase(docs_.begin() + active_);
        if (docs_.empty()) {
            needNew = true;
        } else {
            if (active_ >= docs_.size()) active_ = docs_.size() - 1;
        }
    }
    if (needNew) newDocument();
    return true;
}

// Close the document at the given index
// Verilen indeksteki belgeyi kapat
bool Buffers::closeAt(size_t index) {
    bool needNew = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index >= docs_.size()) return false;
        docs_.erase(docs_.begin() + index);
        if (docs_.empty()) {
            needNew = true;
        } else {
            if (active_ >= docs_.size()) active_ = docs_.size() - 1;
        }
    }
    if (needNew) newDocument();
    return true;
}

// Set the active document by index and sync its cursor
// Indekse gore aktif belgeyi ayarla ve imlecini senkronize et
bool Buffers::setActive(size_t index) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= docs_.size()) return false;
    active_ = index;
    docs_[active_]->syncCursor();
    return true;
}

// Switch to the next document (wraps around)
// Sonraki belgeye gec (basa sarar)
bool Buffers::next() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (docs_.empty()) return false;
    active_ = (active_ + 1) % docs_.size();
    docs_[active_]->syncCursor();
    return true;
}

// Switch to the previous document (wraps around)
// Onceki belgeye gec (basa sarar)
bool Buffers::prev() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (docs_.empty()) return false;
    active_ = (active_ + docs_.size() - 1) % docs_.size();
    docs_[active_]->syncCursor();
    return true;
}

// Return the total number of open documents
// Toplam acik belge sayisini dondur
size_t Buffers::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return docs_.size();
}

// Return the index of the currently active document
// Su an aktif olan belgenin indeksini dondur
size_t Buffers::activeIndex() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_;
}

// Return a mutable reference to the active document's editor state
// Aktif belgenin editor durumuna degistirilebilir referans dondur
EditorState& Buffers::active() {
    std::lock_guard<std::mutex> lock(mutex_);
    return *docs_[active_];
}

// Return a read-only reference to the active document's editor state
// Aktif belgenin editor durumuna salt okunur referans dondur
const EditorState& Buffers::active() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return *docs_[active_];
}

// Return a read-only reference to a document at the given index (does NOT change active)
// Verilen indeksteki belgeye salt okunur referans dondur (aktifi DEGISTIRMEZ)
const EditorState& Buffers::getStateAt(size_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return *docs_[index];
}

// Find an open document by its file path; return its index or nullopt
// Dosya yoluna gore acik bir belge bul; indeksini veya nullopt dondur
std::optional<size_t> Buffers::findByPath(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < docs_.size(); ++i) {
        if (docs_[i]->getFilePath() == path) return i;
    }
    return std::nullopt;
}

// Return the display title (filename) for the document at the given index
// Verilen indeksteki belgenin gorunen basligini (dosya adi) dondur
std::string Buffers::titleOf(size_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= docs_.size()) return {};
    const auto& p = docs_[index]->getFilePath();
    if (p.empty()) return "untitled";
    std::filesystem::path fp(p);
    auto name = fp.filename().string();
    return name.empty() ? p : name;
}

// Extract the filename from a full file path (static utility)
// Tam dosya yolundan dosya adini cikar (statik yardimci fonksiyon)
std::string Buffers::basename(const std::string& path) {
    std::filesystem::path fp(path);
    auto name = fp.filename().string();
    return name.empty() ? path : name;
}
