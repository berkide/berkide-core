#include "IndentEngine.h"
#include "buffer.h"
#include <algorithm>

// Constructor: default 4-space indent
// Kurucu: varsayilan 4 bosluk girinti
IndentEngine::IndentEngine() {}

void IndentEngine::setConfig(const IndentConfig& config) { config_ = config; }
const IndentConfig& IndentEngine::config() const { return config_; }

// Characters that increase indent on the next line
// Sonraki satirda girintiyi artiran karakterler
bool IndentEngine::isIndentIncreaser(char c) const {
    return c == '{' || c == '(' || c == '[' || c == ':';
}

// Characters that decrease indent on the current line
// Mevcut satirda girintiyi azaltan karakterler
bool IndentEngine::isIndentDecreaser(char c) const {
    return c == '}' || c == ')' || c == ']';
}

// Calculate visual width of whitespace string (tabs expand to tabWidth)
// Bosluk dizesinin gorsel genisligini hesapla (sekmeler tabWidth'e genisler)
int IndentEngine::visualWidth(const std::string& ws) const {
    int width = 0;
    for (char c : ws) {
        if (c == '\t') width += config_.tabWidth - (width % config_.tabWidth);
        else width++;
    }
    return width;
}

// Get indent level of a line (visual width / shiftWidth)
// Satirin girinti seviyesini al (gorsel genislik / shiftWidth)
int IndentEngine::getIndentLevel(const std::string& line) const {
    std::string ws = getLeadingWhitespace(line);
    if (config_.shiftWidth <= 0) return 0;
    return visualWidth(ws) / config_.shiftWidth;
}

// Build indent string for a given level
// Verilen seviye icin girinti dizesi olustur
std::string IndentEngine::makeIndentString(int level) const {
    if (level <= 0) return "";
    if (config_.useTabs) {
        return std::string(level, '\t');
    }
    return std::string(level * config_.shiftWidth, ' ');
}

// Get leading whitespace of a line
// Satirin bastaki boslugunu al
std::string IndentEngine::getLeadingWhitespace(const std::string& line) const {
    size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    return line.substr(0, i);
}

// Strip leading whitespace
// Bastaki boslugu cikar
std::string IndentEngine::stripLeadingWhitespace(const std::string& line) const {
    size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    return line.substr(i);
}

// Calculate indent for a new line after afterLine
// afterLine'dan sonraki yeni satir icin girintiyi hesapla
IndentResult IndentEngine::indentForNewLine(const Buffer& buf, int afterLine) const {
    IndentResult result;

    if (afterLine < 0 || afterLine >= buf.lineCount()) return result;

    // If custom indenter set by JS plugin, use it
    // JS eklentisi tarafindan ozel girintici ayarlandiysa kullan
    if (customIndenter_) {
        result.level = customIndenter_(buf, afterLine + 1);
        result.indentString = makeIndentString(result.level);
        return result;
    }

    // Default logic: match previous line indent, increase after opener
    // Varsayilan mantik: onceki satir girintisini esle, acicidan sonra artir
    std::string prevLine = buf.getLine(afterLine);
    int baseLevel = getIndentLevel(prevLine);

    // Check if previous line ends with an indent increaser
    // Onceki satirin girinti artirici ile bitip bitmedigini kontrol et
    std::string stripped = stripLeadingWhitespace(prevLine);
    if (!stripped.empty()) {
        // Find last non-whitespace character
        // Son bosluk olmayan karakteri bul
        char lastChar = 0;
        for (int i = static_cast<int>(stripped.size()) - 1; i >= 0; --i) {
            if (stripped[i] != ' ' && stripped[i] != '\t') {
                lastChar = stripped[i];
                break;
            }
        }
        if (isIndentIncreaser(lastChar)) {
            baseLevel++;
        }
    }

    result.level = baseLevel;
    result.indentString = makeIndentString(result.level);
    return result;
}

// Calculate correct indent for a given line (for reindent)
// Verilen satir icin dogru girintiyi hesapla (yeniden girinti icin)
IndentResult IndentEngine::indentForLine(const Buffer& buf, int line) const {
    IndentResult result;

    if (customIndenter_) {
        result.level = customIndenter_(buf, line);
        result.indentString = makeIndentString(result.level);
        return result;
    }

    if (line <= 0) return result;

    // Base: previous line's indent
    // Temel: onceki satirin girintisi
    result = indentForNewLine(buf, line - 1);

    // Decrease if current line starts with a closer
    // Mevcut satir kapayici ile basliyorsa azalt
    std::string curContent = stripLeadingWhitespace(buf.getLine(line));
    if (!curContent.empty() && isIndentDecreaser(curContent[0])) {
        result.level = std::max(0, result.level - 1);
        result.indentString = makeIndentString(result.level);
    }

    return result;
}

// Increase indent of a line by one level
// Bir satirin girintisini bir seviye artir
std::string IndentEngine::increaseIndent(const std::string& line) const {
    std::string ws = getLeadingWhitespace(line);
    std::string content = line.substr(ws.size());
    int level = getIndentLevel(line) + 1;
    return makeIndentString(level) + content;
}

// Decrease indent of a line by one level
// Bir satirin girintisini bir seviye azalt
std::string IndentEngine::decreaseIndent(const std::string& line) const {
    std::string ws = getLeadingWhitespace(line);
    std::string content = line.substr(ws.size());
    int level = std::max(0, getIndentLevel(line) - 1);
    return makeIndentString(level) + content;
}

// Reindent a range of lines in the buffer
// Buffer'daki satir araligini yeniden girintile
void IndentEngine::reindentRange(Buffer& buf, int startLine, int endLine) const {
    endLine = std::min(endLine, buf.lineCount() - 1);
    for (int i = startLine; i <= endLine; ++i) {
        IndentResult indent = indentForLine(buf, i);
        std::string content = stripLeadingWhitespace(buf.getLine(i));
        buf.getLineRef(i) = indent.indentString + content;
    }
}

// Set custom indent callback from JS plugin
// JS eklentisinden ozel girinti geri cagirimini ayarla
void IndentEngine::setCustomIndenter(IndentCallback cb) { customIndenter_ = std::move(cb); }
bool IndentEngine::hasCustomIndenter() const { return static_cast<bool>(customIndenter_); }
