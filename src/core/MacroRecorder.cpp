#include "MacroRecorder.h"
#include "Logger.h"

// Default constructor
// Varsayilan kurucu
MacroRecorder::MacroRecorder() = default;

// Start recording commands into a named register
// Adlandirilmis bir register'a komut kaydetmeye basla
void MacroRecorder::startRecording(const std::string& reg) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (recording_) return; // Already recording / Zaten kayit yapiliyor
    recording_ = true;
    recordingReg_ = reg;
    currentRecording_.clear();
    LOG_INFO("[Macro] Recording started into register '", reg, "'");
}

// Stop recording and save the macro
// Kaydi durdur ve makroyu kaydet
void MacroRecorder::stopRecording() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!recording_) return;
    recording_ = false;
    macros_[recordingReg_] = std::move(currentRecording_);
    LOG_INFO("[Macro] Recording stopped, saved to register '", recordingReg_,
             "' (", macros_[recordingReg_].size(), " commands)");
    recordingReg_.clear();
}

// Record a single command during macro recording
// Makro kaydi sirasinda tek bir komut kaydet
void MacroRecorder::record(const std::string& commandName, const std::string& argsJson) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!recording_) return;

    // Don't record macro control commands to avoid infinite loops
    // Sonsuz dongulerden kacinmak icin makro kontrol komutlarini kaydetme
    if (commandName == "macro.record" || commandName == "macro.stop" ||
        commandName == "macro.play") return;

    currentRecording_.push_back({commandName, argsJson});
}

// Get a stored macro by register name
// Register adina gore saklanan makroyu al
const std::vector<MacroCommand>* MacroRecorder::getMacro(const std::string& reg) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = macros_.find(reg);
    if (it != macros_.end()) return &it->second;
    return nullptr;
}

// List all macro register names
// Tum makro register adlarini listele
std::vector<std::string> MacroRecorder::listRegisters() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(macros_.size());
    for (auto& [name, cmds] : macros_) {
        result.push_back(name);
    }
    return result;
}

// Clear a specific register
// Belirli bir register'i temizle
bool MacroRecorder::clearRegister(const std::string& reg) {
    std::lock_guard<std::mutex> lock(mutex_);
    return macros_.erase(reg) > 0;
}

// Clear all macros
// Tum makrolari temizle
void MacroRecorder::clearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    macros_.clear();
}
