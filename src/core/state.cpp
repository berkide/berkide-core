// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "state.h"

// Constructor: initialize editor state with Normal mode
// Kurucu: editor durumunu Normal modda baslat
EditorState::EditorState()
    : mode_(EditMode::Normal) {} // Baslangic modu Normal

// Set the current editor mode (Normal, Insert, Visual, etc.)
// Mevcut editor modunu ayarla (Normal, Insert, Visual, vb.)
void EditorState::setMode(EditMode m) {
    mode_ = m;
}

// Get the current editor mode
// Mevcut editor modunu dondur
EditMode EditorState::getMode() const {
    return mode_;
}

// Return mutable references to core components (buffer, cursor, undo)
// Ana bilesenler icin degistirilebilir referanslar dondur (tampon, imlec, geri al)
Buffer& EditorState::getBuffer() {
    return buffer_;
}

Cursor& EditorState::getCursor() {
    return cursor_;
}

// Return read-only references to core components
// Ana bilesenler icin salt okunur referanslar dondur
const Buffer& EditorState::getBuffer() const {
    return buffer_;
}

const Cursor& EditorState::getCursor() const {
    return cursor_;
}

// Return mutable reference to the selection
// Secim icin degistirilebilir referans dondur
Selection& EditorState::getSelection() {
    return selection_;
}

// Return read-only reference to the selection
// Secim icin salt okunur referans dondur
const Selection& EditorState::getSelection() const {
    return selection_;
}

// Return a reference to the undo manager
// Geri alma yoneticisine referans dondur
UndoManager& EditorState::getUndo() {
    return undo_;
}

// Mark the file as modified or unmodified
// Dosyayi degistirilmis veya degistirilmemis olarak isaretle
void EditorState::markModified(bool state) {
    status_.modified = state;
}

// Check if the file has unsaved modifications
// Dosyanin kaydedilmemis degisiklikleri olup olmadigini kontrol et
bool EditorState::isModified() const {
    return status_.modified;
}

// Set the file path associated with this editor state
// Bu editor durumuna bagli dosya yolunu ayarla
void EditorState::setFilePath(const std::string& path) {
    status_.filePath = path;
}

// Get the file path associated with this editor state
// Bu editor durumuna bagli dosya yolunu dondur
const std::string& EditorState::getFilePath() const {
    return status_.filePath;
}


// Reset the entire editor state: clear buffer, cursor, undo, and file info
// Tum editor durumunu sifirla: tampon, imlec, geri alma ve dosya bilgisi
void EditorState::reset() {
    buffer_.clear();            // Tüm metni temizle
    cursor_.setPosition(0, 0);  // İmleci başa al
    selection_.clear();         // Secimi temizle
    undo_ = UndoManager();      // Undo geçmişini sıfırla
    status_ = {};               // Dosya durumunu sıfırla
    mode_ = EditMode::Normal;   // Modu tekrar normal yap
}

// Sync cursor position to stay within buffer boundaries
// Imlec konumunu tampon sinirlarinda kalmasi icin senkronize et
void EditorState::syncCursor() {
    cursor_.clampToBuffer(buffer_);
}