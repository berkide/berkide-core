#include "RegisterManager.h"
#include "Logger.h"

// Default constructor: initialize unnamed register
// Varsayilan kurucu: adsiz register'i baslat
RegisterManager::RegisterManager() {
    registers_["\""] = {"", false};
}

// Set content of a named register
// Adlandirilmis bir register'in icerigini ayarla
void RegisterManager::set(const std::string& name, const std::string& content, bool linewise) {
    if (name == "_") return; // Black hole register discards / Kara delik register'i atar

    std::lock_guard<std::mutex> lock(mutex_);
    registers_[name] = {content, linewise};
}

// Get content of a named register
// Adlandirilmis bir register'in icerigini al
RegisterEntry RegisterManager::get(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = registers_.find(name);
    if (it != registers_.end()) return it->second;
    return {"", false};
}

// Record a yank operation: stores in unnamed and register 0
// Kopyalama islemini kaydet: adsiz ve 0 register'ina sakla
void RegisterManager::recordYank(const std::string& content, bool linewise) {
    std::lock_guard<std::mutex> lock(mutex_);
    registers_["\""] = {content, linewise};  // Unnamed register / Adsiz register
    registers_["0"]  = {content, linewise};  // Yank register / Kopyalama register'i
    LOG_DEBUG("[Register] Yank recorded (", content.size(), " bytes, linewise=", linewise, ")");
}

// Record a delete operation: stores in unnamed and shifts numbered 1-9
// Silme islemini kaydet: adsiz ve numarali 1-9 kaydirmasi yap
void RegisterManager::recordDelete(const std::string& content, bool linewise) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Shift numbered registers: 9<-8<-7...<-2<-1
    // Numarali register'lari kaydir: 9<-8<-7...<-2<-1
    for (int i = 9; i > 1; --i) {
        std::string from = std::to_string(i - 1);
        std::string to   = std::to_string(i);
        auto it = registers_.find(from);
        if (it != registers_.end()) {
            registers_[to] = it->second;
        }
    }

    registers_["1"]  = {content, linewise};  // Most recent delete / En son silme
    registers_["\""] = {content, linewise};  // Unnamed register / Adsiz register
    LOG_DEBUG("[Register] Delete recorded (", content.size(), " bytes, linewise=", linewise, ")");
}

// Get the unnamed register (default yank/delete target)
// Adsiz register'i al (varsayilan kopyalama/silme hedefi)
RegisterEntry RegisterManager::getUnnamed() const {
    return get("\"");
}

// List all non-empty registers
// Tum dolu register'lari listele
std::vector<std::pair<std::string, RegisterEntry>> RegisterManager::list() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<std::string, RegisterEntry>> result;
    result.reserve(registers_.size());
    for (auto& [name, entry] : registers_) {
        if (!entry.content.empty()) {
            result.emplace_back(name, entry);
        }
    }
    return result;
}

// Clear all registers
// Tum register'lari temizle
void RegisterManager::clearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    registers_.clear();
    registers_["\""] = {"", false};
}
