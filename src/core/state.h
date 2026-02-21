#pragma once
#include <memory>
#include <string>
#include "buffer.h"
#include "cursor.h"
#include "undo.h"
#include "Selection.h"

// Editor working modes: Normal (navigation), Insert (typing), Visual (selection)
// Editor calisma modlari: Normal (gezinme), Insert (yazma), Visual (secim)
enum class EditMode { Normal, Insert, Visual };

// File status information (modified flag, readonly flag, file path)
// Dosya durum bilgileri (degistirildi bayragi, salt okunur bayragi, dosya yolu)
struct EditorStatus {
    bool modified = false;      // Whether the file has unsaved changes / Dosyada kaydedilmemis degisiklik var mi
    bool readonly = false;      // Whether the file is read-only / Dosya salt okunur mu
    std::string filePath;       // Path of the currently open file / Acik olan dosyanin yolu
};

// Represents the complete state of a single editor document.
// Tek bir editor belgesinin tam durumunu temsil eder.
// Holds the buffer (text), cursor, undo system, mode, and file status.
// Buffer (metin), imlec, geri alma sistemi, mod ve dosya durumunu tutar.
class EditorState {
public:
    EditorState();

    // Set the current editing mode (Normal/Insert/Visual)
    // Mevcut duzenleme modunu ayarla (Normal/Insert/Visual)
    void setMode(EditMode m);

    // Get the current editing mode
    // Mevcut duzenleme modunu al
    EditMode getMode() const;

    // Access the text buffer (mutable)
    // Metin buffer'ina eris (degistirilebilir)
    Buffer& getBuffer();

    // Access the text buffer (read-only)
    // Metin buffer'ina eris (salt okunur)
    const Buffer& getBuffer() const;

    // Access the cursor (mutable)
    // Imlece eris (degistirilebilir)
    Cursor& getCursor();

    // Access the cursor (read-only)
    // Imlece eris (salt okunur)
    const Cursor& getCursor() const;

    // Access the selection (mutable)
    // Secime eris (degistirilebilir)
    Selection& getSelection();

    // Access the selection (read-only)
    // Secime eris (salt okunur)
    const Selection& getSelection() const;

    // Access the undo manager
    // Geri alma yoneticisine eris
    UndoManager& getUndo();

    // Mark the document as modified or unmodified
    // Belgeyi degistirilmis veya degistirilmemis olarak isaretle
    void markModified(bool state = true);

    // Check if the document has unsaved changes
    // Belgede kaydedilmemis degisiklik olup olmadigini kontrol et
    bool isModified() const;

    // Set the file path associated with this document
    // Bu belgeyle iliskili dosya yolunu ayarla
    void setFilePath(const std::string& path);

    // Get the file path associated with this document
    // Bu belgeyle iliskili dosya yolunu al
    const std::string& getFilePath() const;

    // Reset entire state to a clean new document
    // Tum durumu temiz yeni bir belgeye sifirla
    void reset();

    // Clamp cursor position to valid buffer bounds
    // Imlec konumunu gecerli buffer sinirlarina hizala
    void syncCursor();

private:
    Buffer buffer_;            // Text content / Metin icerigi
    Cursor cursor_;            // Cursor position / Imlec konumu
    Selection selection_;      // Active text selection / Etkin metin secimi
    UndoManager undo_;         // Undo/redo system / Geri al/yinele sistemi
    EditMode mode_;            // Current editing mode / Mevcut duzenleme modu
    EditorStatus status_;      // File status info / Dosya durum bilgileri
};
