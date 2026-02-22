// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include <vector>
#include <memory>

// Regex match result (position and captured groups)
// Regex esleme sonucu (konum ve yakalanan gruplar)
struct RegexMatch {
    int position = -1;           // Start position in string / Dizedeki baslangic konumu
    int length = 0;              // Match length / Esleme uzunlugu
    std::vector<std::string> groups;  // Captured groups / Yakalanan gruplar
};

// Abstract regex engine interface for pluggable backends.
// Taklinabilir arka uclar icin soyut regex motoru arayuzu.
// Default: std::regex. Optional: RE2, PCRE2 (compile with BERKIDE_USE_RE2).
// Varsayilan: std::regex. Opsiyonel: RE2, PCRE2 (BERKIDE_USE_RE2 ile derle).
class RegexEngine {
public:
    virtual ~RegexEngine() = default;

    // Compile a regex pattern with options
    // Seceneklerle bir regex kalibi derle
    virtual bool compile(const std::string& pattern, bool caseSensitive = true) = 0;

    // Check if the engine has a valid compiled pattern
    // Motorun gecerli bir derlenmis kalibi olup olmadigini kontrol et
    virtual bool isValid() const = 0;

    // Find the first match in a string starting from offset
    // Bir dizede ofset'ten baslayarak ilk eslemeyi bul
    virtual bool search(const std::string& text, int offset, RegexMatch& match) const = 0;

    // Find all matches in a string
    // Bir dizedeki tum eslemeleri bul
    virtual std::vector<RegexMatch> searchAll(const std::string& text) const = 0;

    // Replace first match in text
    // Metindeki ilk eslemeyi degistir
    virtual std::string replaceFirst(const std::string& text, const std::string& replacement) const = 0;

    // Replace all matches in text
    // Metindeki tum eslemeleri degistir
    virtual std::string replaceAll(const std::string& text, const std::string& replacement) const = 0;

    // Get last error message (empty if no error)
    // Son hata mesajini al (hata yoksa bos)
    virtual std::string lastError() const = 0;

    // Factory: create the best available regex engine
    // Fabrika: mevcut en iyi regex motorunu olustur
    static std::unique_ptr<RegexEngine> create();
};

// Default implementation using std::regex (C++11 standard library)
// std::regex kullanan varsayilan uygulama (C++11 standart kutuphanesi)
class StdRegexEngine : public RegexEngine {
public:
    StdRegexEngine() = default;

    bool compile(const std::string& pattern, bool caseSensitive = true) override;
    bool isValid() const override;
    bool search(const std::string& text, int offset, RegexMatch& match) const override;
    std::vector<RegexMatch> searchAll(const std::string& text) const override;
    std::string replaceFirst(const std::string& text, const std::string& replacement) const override;
    std::string replaceAll(const std::string& text, const std::string& replacement) const override;
    std::string lastError() const override;

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
    std::string error_;
};
