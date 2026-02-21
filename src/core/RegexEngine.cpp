#include "RegexEngine.h"
#include <regex>

// Internal state for std::regex-based engine
// std::regex tabanli motor icin dahili durum
struct StdRegexEngine::Impl {
    std::regex re;
    bool valid = false;
};

// Factory: create the best available regex engine
// Fabrika: mevcut en iyi regex motorunu olustur
// Currently returns StdRegexEngine. When RE2 is available (BERKIDE_USE_RE2),
// this will return RE2Engine instead for better performance.
// Su an StdRegexEngine dondurur. RE2 mevcut oldugunda (BERKIDE_USE_RE2),
// daha iyi performans icin RE2Engine dondurecek.
std::unique_ptr<RegexEngine> RegexEngine::create() {
    return std::make_unique<StdRegexEngine>();
}

// Compile a regex pattern with the given case sensitivity option
// Verilen buyuk/kucuk harf duyarlilik seçenegiyle bir regex kalibi derle
bool StdRegexEngine::compile(const std::string& pattern, bool caseSensitive) {
    impl_ = std::make_shared<Impl>();
    error_.clear();
    try {
        auto flags = std::regex_constants::ECMAScript;
        if (!caseSensitive) flags |= std::regex_constants::icase;
        impl_->re = std::regex(pattern, flags);
        impl_->valid = true;
        return true;
    } catch (const std::regex_error& e) {
        error_ = e.what();
        impl_->valid = false;
        return false;
    }
}

// Check if engine has a valid compiled regex
// Motorun gecerli bir derlenmis regex'i olup olmadigini kontrol et
bool StdRegexEngine::isValid() const {
    return impl_ && impl_->valid;
}

// Search for first match starting from offset
// Ofset'ten baslayarak ilk eslemeyi ara
bool StdRegexEngine::search(const std::string& text, int offset, RegexMatch& match) const {
    if (!impl_ || !impl_->valid) return false;
    if (offset < 0 || offset >= static_cast<int>(text.size())) return false;

    std::string sub = text.substr(static_cast<size_t>(offset));
    std::smatch m;
    if (std::regex_search(sub, m, impl_->re)) {
        match.position = offset + static_cast<int>(m.position(0));
        match.length = static_cast<int>(m.length(0));
        match.groups.clear();
        for (size_t i = 0; i < m.size(); ++i) {
            match.groups.push_back(m[i].str());
        }
        return true;
    }
    return false;
}

// Find all matches in the text
// Metindeki tum eslemeleri bul
std::vector<RegexMatch> StdRegexEngine::searchAll(const std::string& text) const {
    std::vector<RegexMatch> results;
    if (!impl_ || !impl_->valid) return results;

    int offset = 0;
    RegexMatch m;
    while (search(text, offset, m)) {
        results.push_back(m);
        offset = m.position + std::max(1, m.length);
    }
    return results;
}

// Replace the first match in text
// Metindeki ilk eslemeyi degistir
std::string StdRegexEngine::replaceFirst(const std::string& text, const std::string& replacement) const {
    if (!impl_ || !impl_->valid) return text;
    try {
        return std::regex_replace(text, impl_->re, replacement,
                                   std::regex_constants::format_first_only);
    } catch (...) {
        return text;
    }
}

// Replace all matches in text
// Metindeki tum eslemeleri degistir
std::string StdRegexEngine::replaceAll(const std::string& text, const std::string& replacement) const {
    if (!impl_ || !impl_->valid) return text;
    try {
        return std::regex_replace(text, impl_->re, replacement);
    } catch (...) {
        return text;
    }
}

// Get the last error message
// Son hata mesajini al
std::string StdRegexEngine::lastError() const {
    return error_;
}
