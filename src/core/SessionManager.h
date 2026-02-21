#pragma once
#include <string>
#include <vector>
#include <mutex>

class Buffers;
class WindowManager;

// Information about a single document in the session
// Oturumdaki tek bir belge hakkinda bilgi
struct SessionDocument {
    std::string filePath;    // File path / Dosya yolu
    int cursorLine = 0;      // Cursor line position / Imlec satir konumu
    int cursorCol = 0;       // Cursor column position / Imlec sutun konumu
    int scrollTop = 0;       // Scroll position / Kayma konumu
    bool isActive = false;   // Whether this is the active document / Aktif belge olup olmadigi
};

// Complete session state
// Tam oturum durumu
struct SessionState {
    std::vector<SessionDocument> documents;    // Open documents / Acik belgeler
    int activeIndex = 0;                       // Active document index / Aktif belge indeksi
    std::string lastWorkingDir;                // Last working directory / Son calisma dizini
    int windowWidth = 80;                      // Window width / Pencere genisligi
    int windowHeight = 24;                     // Window height / Pencere yuksekligi
};

// Manages session persistence: save/restore open files, cursors, layout across restarts.
// Oturum kaliciligi yonetir: yeniden baslatmalarda acik dosyalari, imlecleri, duzeni kaydet/geri yukle.
// Session data is stored as JSON in ~/.berkide/session.json.
// Oturum verileri ~/.berkide/session.json dosyasinda JSON olarak saklanir.
class SessionManager {
public:
    SessionManager();

    // Set the session file path (default: ~/.berkide/session.json)
    // Oturum dosyasi yolunu ayarla (varsayilan: ~/.berkide/session.json)
    void setSessionPath(const std::string& path);

    // Save current session state to disk
    // Mevcut oturum durumunu diske kaydet
    bool save(const Buffers& buffers);

    // Load session state from disk
    // Oturum durumunu diskten yukle
    bool load(SessionState& state);

    // Save a custom session state to a named slot
    // Ozel oturum durumunu adlandirilmis bir yuvaya kaydet
    bool saveAs(const std::string& name, const Buffers& buffers);

    // Load a named session
    // Adlandirilmis bir oturumu yukle
    bool loadFrom(const std::string& name, SessionState& state);

    // List available saved sessions
    // Mevcut kaydedilmis oturumlari listele
    std::vector<std::string> listSessions() const;

    // Delete a named session
    // Adlandirilmis bir oturumu sil
    bool deleteSession(const std::string& name);

    // Get the last saved session state (in-memory)
    // Son kaydedilen oturum durumunu al (bellek ici)
    const SessionState& lastState() const;

private:
    mutable std::mutex mutex_;         // Thread safety / Thread guvenligi
    std::string sessionPath_;          // Path to session.json / session.json yolu
    std::string sessionDir_;           // Directory for named sessions / Adlandirilmis oturumlar icin dizin
    SessionState lastState_;           // Last loaded/saved state / Son yuklenen/kaydedilen durum

    // Serialize session state to JSON string
    // Oturum durumunu JSON dizesine serize et
    std::string toJson(const SessionState& state) const;

    // Deserialize JSON string to session state
    // JSON dizesini oturum durumuna geri al
    bool fromJson(const std::string& json, SessionState& state) const;

    // Build session state from current editor
    // Mevcut editordan oturum durumu olustur
    SessionState buildState(const Buffers& buffers) const;
};
