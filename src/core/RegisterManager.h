#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

// Represents a single register entry (stored text and its type)
// Tek bir register girisini temsil eder (saklanan metin ve turu)
struct RegisterEntry {
    std::string content;         // Stored text / Saklanan metin
    bool linewise = false;       // Whether the text is line-wise (ends with newline) / Metnin satir bazli olup olmadigi
};

// Manages named registers for yank/paste operations.
// Kopyalama/yapistirma islemleri icin adlandirilmis register'lari yonetir.
//
// Register naming convention (follows Vim):
// Register adlandirma kurali (Vim'i takip eder):
//   a-z : Named registers (user storage) / Adlandirilmis register'lar (kullanici deposu)
//   0   : Last yank register / Son kopyalama register'i
//   1-9 : Delete history (1=most recent, shifted on each delete) / Silme gecmisi (1=en yeni)
//   "   : Unnamed register (default yank/delete target) / Adsiz register (varsayilan hedef)
//   +   : System clipboard register / Sistem panosu register'i
//   _   : Black hole register (discards text) / Kara delik register'i (metni atar)
class RegisterManager {
public:
    RegisterManager();

    // Set content of a named register
    // Adlandirilmis bir register'in icerigini ayarla
    void set(const std::string& name, const std::string& content, bool linewise = false);

    // Get content of a named register (empty string if not set)
    // Adlandirilmis bir register'in icerigini al (ayarlanmamissa bos dize)
    RegisterEntry get(const std::string& name) const;

    // Record a yank operation: stores in unnamed register and register 0
    // Kopyalama islemini kaydet: adsiz register ve 0 register'ina sakla
    void recordYank(const std::string& content, bool linewise = false);

    // Record a delete operation: stores in unnamed register and shifts 1-9
    // Silme islemini kaydet: adsiz register ve 1-9 kaydirmasi yap
    void recordDelete(const std::string& content, bool linewise = false);

    // Get the unnamed register (last yank or delete)
    // Adsiz register'i al (son kopyalama veya silme)
    RegisterEntry getUnnamed() const;

    // List all non-empty registers as name->content pairs
    // Tum dolu register'lari isim->icerik cifti olarak listele
    std::vector<std::pair<std::string, RegisterEntry>> list() const;

    // Clear all registers
    // Tum register'lari temizle
    void clearAll();

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, RegisterEntry> registers_;
};
