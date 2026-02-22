// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

// A single recorded command in a macro sequence
// Makro sirasindaki tek bir kaydedilmis komut
struct MacroCommand {
    std::string name;            // Command name / Komut adi
    std::string argsJson;        // Command arguments as JSON / JSON olarak komut argumanlari
};

// Records and plays back sequences of editor commands.
// Editor komut dizilerini kaydeder ve yeniden oynatir.
// Macros are stored in named registers (like Vim: qa...q, @a).
// Makrolar adlandirilmis register'larda saklanir (Vim gibi: qa...q, @a).
class MacroRecorder {
public:
    MacroRecorder();

    // Start recording commands into a named register
    // Adlandirilmis bir register'a komut kaydetmeye basla
    void startRecording(const std::string& reg);

    // Stop recording and save the macro
    // Kaydi durdur ve makroyu kaydet
    void stopRecording();

    // Check if currently recording
    // Su anda kayit yapilip yapilmadigini kontrol et
    bool isRecording() const { return recording_; }

    // Get the register being recorded into
    // Kayit yapilan register'i al
    const std::string& recordingRegister() const { return recordingReg_; }

    // Record a single command (called by command router during recording)
    // Tek bir komut kaydet (kayit sirasinda komut yonlendiricisi tarafindan cagirilir)
    void record(const std::string& commandName, const std::string& argsJson);

    // Get a macro by register name
    // Register adina gore makro al
    const std::vector<MacroCommand>* getMacro(const std::string& reg) const;

    // List all stored macro register names
    // Tum saklanan makro register adlarini listele
    std::vector<std::string> listRegisters() const;

    // Clear a specific macro register
    // Belirli bir makro register'ini temizle
    bool clearRegister(const std::string& reg);

    // Clear all macros
    // Tum makrolari temizle
    void clearAll();

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<MacroCommand>> macros_;
    bool recording_ = false;
    std::string recordingReg_;
    std::vector<MacroCommand> currentRecording_;
};
