// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include <functional>

class Buffer;

// Indent style configuration
// Girinti stili yapilandirmasi
struct IndentConfig {
    bool useTabs = false;
    int tabWidth = 4;
    int shiftWidth = 4;
};

// Result of an indent calculation
// Girinti hesaplamasinin sonucu
struct IndentResult {
    int level = 0;
    std::string indentString;
};

// Core auto-indent engine: basic indent logic + JS plugin callback for language rules.
// Temel otomatik girinti motoru: temel girinti mantigi + dil kurallari icin JS eklenti geri cagrimi.
class IndentEngine {
public:
    IndentEngine();

    void setConfig(const IndentConfig& config);
    const IndentConfig& config() const;

    // Calculate indent for a new line after the given line
    // Verilen satirdan sonraki yeni satir icin girintiyi hesapla
    IndentResult indentForNewLine(const Buffer& buf, int afterLine) const;

    // Calculate correct indent for a given line (reindent)
    // Verilen satir icin dogru girintiyi hesapla (yeniden girinti)
    IndentResult indentForLine(const Buffer& buf, int line) const;

    // Get indent level of a line
    // Bir satirin girinti seviyesini al
    int getIndentLevel(const std::string& line) const;

    // Build indent string for a given level
    // Verilen seviye icin girinti dizesi olustur
    std::string makeIndentString(int level) const;

    // Get leading whitespace / strip it
    // Bastaki boslugu al / cikar
    std::string getLeadingWhitespace(const std::string& line) const;
    std::string stripLeadingWhitespace(const std::string& line) const;

    // Increase/decrease indent of a line
    // Bir satirin girintisini artir/azalt
    std::string increaseIndent(const std::string& line) const;
    std::string decreaseIndent(const std::string& line) const;

    // Reindent a range of lines
    // Satir araligini yeniden girintile
    void reindentRange(Buffer& buf, int startLine, int endLine) const;

    // JS plugin can override default indent logic
    // JS eklentisi varsayilan girinti mantigini geersiz kilabilir
    using IndentCallback = std::function<int(const Buffer& buf, int line)>;
    void setCustomIndenter(IndentCallback cb);
    bool hasCustomIndenter() const;

private:
    IndentConfig config_;
    IndentCallback customIndenter_;

    bool isIndentIncreaser(char c) const;
    bool isIndentDecreaser(char c) const;
    int visualWidth(const std::string& whitespace) const;
};
