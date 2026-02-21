#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>

// Virtual text position relative to the extmark range
// Extmark araligina gore sanal metin konumu
enum class VirtTextPos {
    None,       // No virtual text / Sanal metin yok
    EOL,        // After end of line / Satir sonundan sonra
    Inline,     // Inline at start position / Baslangic konumunda satir ici
    Overlay,    // Overlay on top of existing text / Mevcut metnin uzerine
    RightAlign  // Right-aligned at end of line / Satir sonunda saga yasli
};

// A single extmark: metadata attached to a text range.
// Tek bir extmark: bir metin araligina eklenmis meta veri.
// Used by syntax highlighting, diagnostics, git gutter, bracket matching, etc.
// Soz dizimi vurgulama, tanilar, git sutunu, parantez esleme vb. tarafindan kullanilir.
struct Extmark {
    int id = 0;                  // Unique mark ID / Benzersiz isaret kimigi
    int startLine = 0;           // Start line / Baslangic satiri
    int startCol  = 0;           // Start column / Baslangic sutunu
    int endLine   = 0;           // End line / Bitis satiri
    int endCol    = 0;           // End column / Bitis sutunu
    std::string ns;              // Namespace (e.g., "syntax", "lint", "git") / Ad alani
    std::string type;            // Mark type within namespace / Ad alani icindeki isaret turu
    std::string data;            // Arbitrary JSON data / Keyfi JSON verisi

    // Virtual text fields (like Neovim virt_text, VS Code InlineDecoration)
    // Sanal metin alanlari (Neovim virt_text, VS Code InlineDecoration gibi)
    std::string virtText;                    // Text to display virtually / Sanal olarak goruntulenecek metin
    VirtTextPos virtTextPos = VirtTextPos::None;  // Position of virtual text / Sanal metnin konumu
    std::string virtTextStyle;               // Style/highlight group for virtual text / Sanal metin icin stil/vurgulama grubu
};

// Manages text decorations/properties attached to buffer ranges.
// Buffer araliklarimna eklenmis metin dekorasyonlarini/ozelliklerini yonetir.
// Extmarks auto-adjust when text is edited (insert/delete shifts positions).
// Extmark'lar metin duzenlendiginde otomatik olarak ayarlanir.
// Namespaces isolate different producers (syntax vs lint vs git don't collide).
// Ad alanlari farkli ureticileri izole eder (syntax vs lint vs git carpismaz).
class ExtmarkManager {
public:
    ExtmarkManager();

    // Add a new extmark and return its ID
    // Yeni bir extmark ekle ve kimligini dondur
    int set(const std::string& ns, int startLine, int startCol,
            int endLine, int endCol,
            const std::string& type = "", const std::string& data = "");

    // Add a new extmark with virtual text and return its ID
    // Sanal metinli yeni bir extmark ekle ve kimligini dondur
    int setWithVirtText(const std::string& ns, int startLine, int startCol,
                        int endLine, int endCol,
                        const std::string& virtText, VirtTextPos virtPos,
                        const std::string& virtStyle = "",
                        const std::string& type = "", const std::string& data = "");

    // Get a single extmark by ID
    // Kimlige gore tek bir extmark al
    const Extmark* get(int id) const;

    // Delete an extmark by ID
    // Kimlige gore bir extmark'i sil
    bool remove(int id);

    // Delete all extmarks in a namespace
    // Bir ad alanindaki tum extmark'lari sil
    int clearNamespace(const std::string& ns);

    // Get all extmarks that overlap a line range
    // Bir satir araligini kesen tum extmark'lari al
    std::vector<const Extmark*> getInRange(int startLine, int endLine,
                                            const std::string& ns = "") const;

    // Get all extmarks on a specific line
    // Belirli bir satirdaki tum extmark'lari al
    std::vector<const Extmark*> getOnLine(int line, const std::string& ns = "") const;

    // Adjust extmark positions after a text edit
    // Bir metin duzenlemesinden sonra extmark konumlarini ayarla
    // Called after buffer insert/delete to keep marks in sync.
    // Isaretleri senkronize tutmak icin buffer ekleme/silme sonrasi cagirilir.
    void adjustForInsert(int line, int col, int linesAdded, int colsAdded);
    void adjustForDelete(int startLine, int startCol, int endLine, int endCol);

    // List all extmarks (optionally filtered by namespace)
    // Tum extmark'lari listele (istege bagli olarak ad alanina gore filtrele)
    std::vector<const Extmark*> list(const std::string& ns = "") const;

    // Get total count of extmarks
    // Toplam extmark sayisini al
    size_t count() const;

    // Clear all extmarks
    // Tum extmark'lari temizle
    void clearAll();

private:
    mutable std::mutex mutex_;
    std::unordered_map<int, Extmark> marks_;
    std::atomic<int> nextId_{1};
};
