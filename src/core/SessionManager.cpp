// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "SessionManager.h"
#include "buffers.h"
#include "Logger.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

// Constructor: set default session path
// Kurucu: varsayilan oturum yolunu ayarla
SessionManager::SessionManager() {
    // Default path set later via setSessionPath()
    // Varsayilan yol daha sonra setSessionPath() ile ayarlanir
}

// Set the session file path
// Oturum dosyasi yolunu ayarla
void SessionManager::setSessionPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessionPath_ = path;
    sessionDir_ = (fs::path(path).parent_path() / "sessions").string();
}

// Build session state from current editor
// Mevcut editordan oturum durumu olustur
SessionState SessionManager::buildState(const Buffers& buffers) const {
    SessionState state;
    size_t count = buffers.count();
    size_t activeIdx = buffers.activeIndex();

    for (size_t i = 0; i < count; ++i) {
        // Use const reference to access each document
        // Her belgeye erismek icin const referans kullan
        // We need to temporarily switch active to read each doc's state
        // Her belgenin durumunu okumak icin gecici olarak aktifi degistirmemiz gerekiyor
        // Since we only have const access, we read what we can
        // Sadece const erisimimiz oldugu icin okuyabildiklerimizi okuruz
    }

    // With const Buffers& we can only read the active document's full state
    // const Buffers& ile sadece aktif belgenin tam durumunu okuyabiliriz
    const auto& active = buffers.active();

    for (size_t i = 0; i < count; ++i) {
        SessionDocument doc;
        doc.filePath = buffers.titleOf(i);
        doc.isActive = (i == activeIdx);

        if (i == activeIdx) {
            doc.cursorLine = active.getCursor().getLine();
            doc.cursorCol = active.getCursor().getCol();
            doc.filePath = active.getFilePath();
        }

        // Only add documents that have actual file paths
        // Sadece gercek dosya yollari olan belgeleri ekle
        if (!doc.filePath.empty() && doc.filePath != "untitled") {
            state.documents.push_back(doc);
        }
    }

    state.activeIndex = static_cast<int>(activeIdx);

    // Current working directory
    // Mevcut calisma dizini
    try {
        state.lastWorkingDir = fs::current_path().string();
    } catch (...) {
        state.lastWorkingDir = ".";
    }

    return state;
}

// Serialize session state to JSON string
// Oturum durumunu JSON dizesine seri hale getir
std::string SessionManager::toJson(const SessionState& state) const {
    json j;
    j["version"] = 1;
    j["activeIndex"] = state.activeIndex;
    j["workingDir"] = state.lastWorkingDir;
    j["windowWidth"] = state.windowWidth;
    j["windowHeight"] = state.windowHeight;

    json docs = json::array();
    for (const auto& doc : state.documents) {
        json d;
        d["filePath"] = doc.filePath;
        d["cursorLine"] = doc.cursorLine;
        d["cursorCol"] = doc.cursorCol;
        d["scrollTop"] = doc.scrollTop;
        d["isActive"] = doc.isActive;
        docs.push_back(d);
    }
    j["documents"] = docs;

    return j.dump(2);
}

// Deserialize JSON string to session state
// JSON dizesini oturum durumuna geri al
bool SessionManager::fromJson(const std::string& jsonStr, SessionState& state) const {
    try {
        json j = json::parse(jsonStr);

        state.activeIndex = j.value("activeIndex", 0);
        state.lastWorkingDir = j.value("workingDir", "");
        state.windowWidth = j.value("windowWidth", 80);
        state.windowHeight = j.value("windowHeight", 24);

        state.documents.clear();
        if (j.contains("documents") && j["documents"].is_array()) {
            for (const auto& d : j["documents"]) {
                SessionDocument doc;
                doc.filePath = d.value("filePath", "");
                doc.cursorLine = d.value("cursorLine", 0);
                doc.cursorCol = d.value("cursorCol", 0);
                doc.scrollTop = d.value("scrollTop", 0);
                doc.isActive = d.value("isActive", false);
                state.documents.push_back(doc);
            }
        }

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("[Session] Failed to parse session JSON: ", e.what());
        return false;
    }
}

// Save current session state to disk
// Mevcut oturum durumunu diske kaydet
bool SessionManager::save(const Buffers& buffers) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (sessionPath_.empty()) {
        LOG_WARN("[Session] Session path not set, cannot save.");
        return false;
    }

    SessionState state = buildState(buffers);
    lastState_ = state;

    std::string jsonStr = toJson(state);

    try {
        // Ensure directory exists
        // Dizinin var oldugundan emin ol
        fs::path dir = fs::path(sessionPath_).parent_path();
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
        }

        std::ofstream file(sessionPath_, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            LOG_ERROR("[Session] Cannot write to: ", sessionPath_);
            return false;
        }

        file << jsonStr;
        LOG_INFO("[Session] Session saved: ", state.documents.size(), " documents");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("[Session] Save failed: ", e.what());
        return false;
    }
}

// Load session state from disk
// Oturum durumunu diskten yukle
bool SessionManager::load(SessionState& state) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (sessionPath_.empty()) {
        LOG_WARN("[Session] Session path not set, cannot load.");
        return false;
    }

    try {
        if (!fs::exists(sessionPath_)) {
            LOG_INFO("[Session] No session file found at: ", sessionPath_);
            return false;
        }

        std::ifstream file(sessionPath_);
        if (!file.is_open()) {
            LOG_ERROR("[Session] Cannot read: ", sessionPath_);
            return false;
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        std::string jsonStr = ss.str();

        if (!fromJson(jsonStr, state)) return false;

        lastState_ = state;
        LOG_INFO("[Session] Session loaded: ", state.documents.size(), " documents");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("[Session] Load failed: ", e.what());
        return false;
    }
}

// Save session to a named slot
// Oturumu adlandirilmis yuvaya kaydet
bool SessionManager::saveAs(const std::string& name, const Buffers& buffers) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (sessionDir_.empty()) {
        LOG_WARN("[Session] Session directory not set.");
        return false;
    }

    SessionState state = buildState(buffers);
    std::string jsonStr = toJson(state);

    try {
        fs::create_directories(sessionDir_);
        std::string path = (fs::path(sessionDir_) / (name + ".json")).string();

        std::ofstream file(path, std::ios::out | std::ios::trunc);
        if (!file.is_open()) return false;

        file << jsonStr;
        LOG_INFO("[Session] Named session saved: ", name);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("[Session] SaveAs failed: ", e.what());
        return false;
    }
}

// Load a named session
// Adlandirilmis bir oturumu yukle
bool SessionManager::loadFrom(const std::string& name, SessionState& state) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        std::string path = (fs::path(sessionDir_) / (name + ".json")).string();
        if (!fs::exists(path)) return false;

        std::ifstream file(path);
        std::ostringstream ss;
        ss << file.rdbuf();

        return fromJson(ss.str(), state);
    } catch (const std::exception& e) {
        LOG_ERROR("[Session] LoadFrom failed: ", e.what());
        return false;
    }
}

// List available saved sessions
// Mevcut kaydedilmis oturumlari listele
std::vector<std::string> SessionManager::listSessions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;

    try {
        if (!fs::exists(sessionDir_)) return names;

        for (const auto& entry : fs::directory_iterator(sessionDir_)) {
            if (entry.path().extension() == ".json") {
                names.push_back(entry.path().stem().string());
            }
        }

        std::sort(names.begin(), names.end());
    } catch (...) {}

    return names;
}

// Delete a named session
// Adlandirilmis bir oturumu sil
bool SessionManager::deleteSession(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        std::string path = (fs::path(sessionDir_) / (name + ".json")).string();
        return fs::remove(path);
    } catch (...) {
        return false;
    }
}

// Get the last saved/loaded session state
// Son kaydedilen/yuklenen oturum durumunu al
const SessionState& SessionManager::lastState() const {
    return lastState_;
}
