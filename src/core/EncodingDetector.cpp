#include "EncodingDetector.h"
#include "Logger.h"
#include <fstream>
#include <algorithm>
#include <cstring>

// Detect encoding from a vector of bytes
// Bayt vektorunden kodlamayi algila
EncodingResult EncodingDetector::detect(const std::vector<uint8_t>& data) {
    return detect(data.data(), data.size());
}

// Detect encoding from raw byte pointer and size
// Ham bayt isaretcisi ve boyuttan kodlamayi algila
EncodingResult EncodingDetector::detect(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return {TextEncoding::UTF8, false, 0, 1.0};
    }

    // First check for BOM
    // Once BOM kontrol et
    EncodingResult bomResult = detectBOM(data, size);
    if (bomResult.hasBOM) {
        return bomResult;
    }

    // No BOM found, use heuristic detection
    // BOM bulunamadi, bulussel algilama kullan
    return detectHeuristic(data, size);
}

// Detect encoding of a file on disk
// Diskteki bir dosyanin kodlamasini algila
EncodingResult EncodingDetector::detectFile(const std::string& filePath) {
    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            LOG_ERROR("[Encoding] Cannot open file: ", filePath);
            return {TextEncoding::Unknown, false, 0, 0.0};
        }

        // Read first 64KB for detection (sufficient for heuristics)
        // Algilama icin ilk 64KB oku (buluseller icin yeterli)
        constexpr size_t SAMPLE_SIZE = 65536;
        std::vector<uint8_t> buffer(SAMPLE_SIZE);
        file.read(reinterpret_cast<char*>(buffer.data()), SAMPLE_SIZE);
        size_t bytesRead = static_cast<size_t>(file.gcount());
        buffer.resize(bytesRead);

        return detect(buffer.data(), bytesRead);
    } catch (const std::exception& e) {
        LOG_ERROR("[Encoding] Detection failed: ", e.what());
        return {TextEncoding::Unknown, false, 0, 0.0};
    }
}

// Check for BOM at start of data
// Verinin basindaki BOM'u kontrol et
EncodingResult EncodingDetector::detectBOM(const uint8_t* data, size_t size) {
    EncodingResult r;
    r.hasBOM = false;

    // UTF-32 LE BOM: FF FE 00 00
    if (size >= 4 && data[0] == 0xFF && data[1] == 0xFE && data[2] == 0x00 && data[3] == 0x00) {
        r.encoding = TextEncoding::UTF32_LE;
        r.hasBOM = true;
        r.bomSize = 4;
        r.confidence = 1.0;
        return r;
    }

    // UTF-32 BE BOM: 00 00 FE FF
    if (size >= 4 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0xFE && data[3] == 0xFF) {
        r.encoding = TextEncoding::UTF32_BE;
        r.hasBOM = true;
        r.bomSize = 4;
        r.confidence = 1.0;
        return r;
    }

    // UTF-8 BOM: EF BB BF
    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        r.encoding = TextEncoding::UTF8_BOM;
        r.hasBOM = true;
        r.bomSize = 3;
        r.confidence = 1.0;
        return r;
    }

    // UTF-16 LE BOM: FF FE (but not followed by 00 00, which is UTF-32)
    if (size >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        r.encoding = TextEncoding::UTF16_LE;
        r.hasBOM = true;
        r.bomSize = 2;
        r.confidence = 1.0;
        return r;
    }

    // UTF-16 BE BOM: FE FF
    if (size >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
        r.encoding = TextEncoding::UTF16_BE;
        r.hasBOM = true;
        r.bomSize = 2;
        r.confidence = 1.0;
        return r;
    }

    return r;
}

// Heuristic encoding detection when no BOM is present
// BOM bulunmadiginda bulussel kodlama algilama
EncodingResult EncodingDetector::detectHeuristic(const uint8_t* data, size_t size) {
    // Check if pure ASCII (all bytes < 128)
    // Saf ASCII mi kontrol et (tum baytlar < 128)
    if (isASCII(data, size)) {
        return {TextEncoding::ASCII, false, 0, 1.0};
    }

    // Check if valid UTF-8
    // Gecerli UTF-8 mi kontrol et
    if (isValidUTF8(data, size)) {
        return {TextEncoding::UTF8, false, 0, 0.95};
    }

    // Check for UTF-16 patterns (every other byte is 0 for ASCII-heavy text)
    // UTF-16 kaliplari kontrol et (ASCII agirlikli metin icin her ikinci bayt 0)
    if (size >= 4) {
        int nullOdd = 0, nullEven = 0;
        size_t checkSize = std::min(size, static_cast<size_t>(1024));
        for (size_t i = 0; i < checkSize; ++i) {
            if (data[i] == 0) {
                if (i % 2 == 0) nullEven++;
                else nullOdd++;
            }
        }

        double ratio = static_cast<double>(checkSize) / 2.0;
        if (nullOdd > ratio * 0.3 && nullEven < ratio * 0.1) {
            return {TextEncoding::UTF16_LE, false, 0, 0.7};
        }
        if (nullEven > ratio * 0.3 && nullOdd < ratio * 0.1) {
            return {TextEncoding::UTF16_BE, false, 0, 0.7};
        }
    }

    // Fallback: assume Latin-1 (ISO-8859-1) since it covers all byte values 0-255
    // Geri donus: tum bayt degerleri 0-255'i kapsadigi icin Latin-1 (ISO-8859-1) varsay
    return {TextEncoding::Latin1, false, 0, 0.5};
}

// Validate UTF-8 encoding
// UTF-8 kodlamasini dogrula
bool EncodingDetector::isValidUTF8(const uint8_t* data, size_t size) {
    size_t i = 0;
    bool hasMultibyte = false;

    while (i < size) {
        if (data[i] < 0x80) {
            // ASCII byte
            i++;
        } else if ((data[i] & 0xE0) == 0xC0) {
            // 2-byte sequence: 110xxxxx 10xxxxxx
            if (i + 1 >= size) return false;
            if ((data[i + 1] & 0xC0) != 0x80) return false;
            // Overlong check: must be >= 0x80
            uint32_t cp = ((data[i] & 0x1F) << 6) | (data[i + 1] & 0x3F);
            if (cp < 0x80) return false;
            hasMultibyte = true;
            i += 2;
        } else if ((data[i] & 0xF0) == 0xE0) {
            // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
            if (i + 2 >= size) return false;
            if ((data[i + 1] & 0xC0) != 0x80) return false;
            if ((data[i + 2] & 0xC0) != 0x80) return false;
            uint32_t cp = ((data[i] & 0x0F) << 12) | ((data[i + 1] & 0x3F) << 6) | (data[i + 2] & 0x3F);
            if (cp < 0x800) return false;
            // Reject surrogates
            if (cp >= 0xD800 && cp <= 0xDFFF) return false;
            hasMultibyte = true;
            i += 3;
        } else if ((data[i] & 0xF8) == 0xF0) {
            // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            if (i + 3 >= size) return false;
            if ((data[i + 1] & 0xC0) != 0x80) return false;
            if ((data[i + 2] & 0xC0) != 0x80) return false;
            if ((data[i + 3] & 0xC0) != 0x80) return false;
            uint32_t cp = ((data[i] & 0x07) << 18) | ((data[i + 1] & 0x3F) << 12) |
                          ((data[i + 2] & 0x3F) << 6) | (data[i + 3] & 0x3F);
            if (cp < 0x10000 || cp > 0x10FFFF) return false;
            hasMultibyte = true;
            i += 4;
        } else {
            // Invalid UTF-8 start byte
            // Gecersiz UTF-8 baslangic bayti
            return false;
        }
    }

    // Only return true if we found multibyte sequences (otherwise it's ASCII, not UTF-8)
    // Sadece coklu bayt dizileri bulduysak true dondur (aksi halde ASCII, UTF-8 degil)
    return hasMultibyte;
}

// Check if data is pure 7-bit ASCII
// Verinin saf 7-bit ASCII olup olmadigini kontrol et
bool EncodingDetector::isASCII(const uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        if (data[i] >= 0x80) return false;
    }
    return true;
}

// Encode a single Unicode code point to UTF-8
// Tek bir Unicode kod noktasini UTF-8'e kodla
void EncodingDetector::encodeUTF8(uint32_t cp, std::string& out) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0x10FFFF) {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        // Invalid code point: emit replacement character U+FFFD
        // Gecersiz kod noktasi: degistirme karakteri U+FFFD yay
        out += "\xEF\xBF\xBD";
    }
}

// Convert UTF-16 bytes to UTF-8 string
// UTF-16 baytlarini UTF-8 dizesine donustur
std::string EncodingDetector::utf16ToUTF8(const uint8_t* data, size_t size, bool littleEndian) {
    std::string result;
    result.reserve(size);

    for (size_t i = 0; i + 1 < size; i += 2) {
        uint16_t unit;
        if (littleEndian) {
            unit = static_cast<uint16_t>(data[i]) | (static_cast<uint16_t>(data[i + 1]) << 8);
        } else {
            unit = (static_cast<uint16_t>(data[i]) << 8) | static_cast<uint16_t>(data[i + 1]);
        }

        if (unit >= 0xD800 && unit <= 0xDBFF) {
            // High surrogate: read low surrogate
            // Yuksek vekil: dusuk vekili oku
            if (i + 3 < size) {
                uint16_t low;
                if (littleEndian) {
                    low = static_cast<uint16_t>(data[i + 2]) | (static_cast<uint16_t>(data[i + 3]) << 8);
                } else {
                    low = (static_cast<uint16_t>(data[i + 2]) << 8) | static_cast<uint16_t>(data[i + 3]);
                }

                if (low >= 0xDC00 && low <= 0xDFFF) {
                    uint32_t cp = 0x10000 + ((static_cast<uint32_t>(unit - 0xD800) << 10) | (low - 0xDC00));
                    encodeUTF8(cp, result);
                    i += 2;
                    continue;
                }
            }
            // Unpaired surrogate: emit replacement
            // Eslenmemis vekil: degistirme karakteri yay
            result += "\xEF\xBF\xBD";
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            // Orphan low surrogate
            // Yetim dusuk vekil
            result += "\xEF\xBF\xBD";
        } else {
            encodeUTF8(static_cast<uint32_t>(unit), result);
        }
    }

    return result;
}

// Convert UTF-32 bytes to UTF-8 string
// UTF-32 baytlarini UTF-8 dizesine donustur
std::string EncodingDetector::utf32ToUTF8(const uint8_t* data, size_t size, bool littleEndian) {
    std::string result;
    result.reserve(size);

    for (size_t i = 0; i + 3 < size; i += 4) {
        uint32_t cp;
        if (littleEndian) {
            cp = static_cast<uint32_t>(data[i]) |
                 (static_cast<uint32_t>(data[i + 1]) << 8) |
                 (static_cast<uint32_t>(data[i + 2]) << 16) |
                 (static_cast<uint32_t>(data[i + 3]) << 24);
        } else {
            cp = (static_cast<uint32_t>(data[i]) << 24) |
                 (static_cast<uint32_t>(data[i + 1]) << 16) |
                 (static_cast<uint32_t>(data[i + 2]) << 8) |
                 static_cast<uint32_t>(data[i + 3]);
        }

        encodeUTF8(cp, result);
    }

    return result;
}

// Convert Latin-1 (ISO-8859-1) bytes to UTF-8
// Latin-1 (ISO-8859-1) baytlarini UTF-8'e donustur
std::string EncodingDetector::latin1ToUTF8(const uint8_t* data, size_t size) {
    std::string result;
    result.reserve(size * 2);

    for (size_t i = 0; i < size; ++i) {
        encodeUTF8(static_cast<uint32_t>(data[i]), result);
    }

    return result;
}

// Convert raw bytes to UTF-8 string using the specified encoding
// Belirtilen kodlamayi kullanarak ham baytlari UTF-8 dizesine donustur
std::string EncodingDetector::toUTF8(const std::vector<uint8_t>& data, TextEncoding encoding) {
    if (data.empty()) return "";

    const uint8_t* ptr = data.data();
    size_t size = data.size();

    // Skip BOM if present
    // Varsa BOM'u atla
    EncodingResult bom = detectBOM(ptr, size);
    if (bom.hasBOM) {
        ptr += bom.bomSize;
        size -= bom.bomSize;
    }

    switch (encoding) {
        case TextEncoding::UTF8:
        case TextEncoding::UTF8_BOM:
        case TextEncoding::ASCII:
            return std::string(reinterpret_cast<const char*>(ptr), size);

        case TextEncoding::UTF16_LE:
            return utf16ToUTF8(ptr, size, true);

        case TextEncoding::UTF16_BE:
            return utf16ToUTF8(ptr, size, false);

        case TextEncoding::UTF32_LE:
            return utf32ToUTF8(ptr, size, true);

        case TextEncoding::UTF32_BE:
            return utf32ToUTF8(ptr, size, false);

        case TextEncoding::Latin1:
            return latin1ToUTF8(ptr, size);

        default:
            // Unknown: try as raw bytes
            // Bilinmeyen: ham bayt olarak dene
            return std::string(reinterpret_cast<const char*>(ptr), size);
    }
}

// Convert UTF-8 string to target encoding bytes
// UTF-8 dizesini hedef kodlama baytlarina donustur
std::vector<uint8_t> EncodingDetector::fromUTF8(const std::string& utf8, TextEncoding encoding) {
    std::vector<uint8_t> result;

    switch (encoding) {
        case TextEncoding::UTF8:
        case TextEncoding::ASCII:
            result.assign(utf8.begin(), utf8.end());
            break;

        case TextEncoding::UTF8_BOM:
            result.push_back(0xEF);
            result.push_back(0xBB);
            result.push_back(0xBF);
            result.insert(result.end(), utf8.begin(), utf8.end());
            break;

        case TextEncoding::Latin1: {
            // UTF-8 to Latin-1: decode code points, clamp to 0-255
            // UTF-8'den Latin-1'e: kod noktalarini coz, 0-255'e sinirla
            size_t i = 0;
            while (i < utf8.size()) {
                uint32_t cp = 0;
                uint8_t b = static_cast<uint8_t>(utf8[i]);
                if (b < 0x80) {
                    cp = b;
                    i++;
                } else if ((b & 0xE0) == 0xC0) {
                    cp = (b & 0x1F) << 6;
                    if (i + 1 < utf8.size()) cp |= (static_cast<uint8_t>(utf8[i + 1]) & 0x3F);
                    i += 2;
                } else if ((b & 0xF0) == 0xE0) {
                    cp = (b & 0x0F) << 12;
                    if (i + 1 < utf8.size()) cp |= (static_cast<uint8_t>(utf8[i + 1]) & 0x3F) << 6;
                    if (i + 2 < utf8.size()) cp |= (static_cast<uint8_t>(utf8[i + 2]) & 0x3F);
                    i += 3;
                } else {
                    i += 4;
                    cp = '?';
                }
                result.push_back(cp <= 0xFF ? static_cast<uint8_t>(cp) : '?');
            }
            break;
        }

        case TextEncoding::UTF16_LE: {
            // Add BOM
            // BOM ekle
            result.push_back(0xFF);
            result.push_back(0xFE);
            // Encode UTF-8 to UTF-16 LE
            // UTF-8'den UTF-16 LE'ye kodla
            size_t i = 0;
            while (i < utf8.size()) {
                uint32_t cp = 0;
                uint8_t b = static_cast<uint8_t>(utf8[i]);
                if (b < 0x80) { cp = b; i++; }
                else if ((b & 0xE0) == 0xC0) {
                    cp = (b & 0x1F) << 6;
                    if (i + 1 < utf8.size()) cp |= (static_cast<uint8_t>(utf8[i + 1]) & 0x3F);
                    i += 2;
                } else if ((b & 0xF0) == 0xE0) {
                    cp = (b & 0x0F) << 12;
                    if (i + 1 < utf8.size()) cp |= (static_cast<uint8_t>(utf8[i + 1]) & 0x3F) << 6;
                    if (i + 2 < utf8.size()) cp |= (static_cast<uint8_t>(utf8[i + 2]) & 0x3F);
                    i += 3;
                } else if ((b & 0xF8) == 0xF0) {
                    cp = (b & 0x07) << 18;
                    if (i + 1 < utf8.size()) cp |= (static_cast<uint8_t>(utf8[i + 1]) & 0x3F) << 12;
                    if (i + 2 < utf8.size()) cp |= (static_cast<uint8_t>(utf8[i + 2]) & 0x3F) << 6;
                    if (i + 3 < utf8.size()) cp |= (static_cast<uint8_t>(utf8[i + 3]) & 0x3F);
                    i += 4;
                } else { i++; continue; }

                if (cp < 0x10000) {
                    result.push_back(static_cast<uint8_t>(cp & 0xFF));
                    result.push_back(static_cast<uint8_t>((cp >> 8) & 0xFF));
                } else {
                    // Surrogate pair
                    // Vekil cifti
                    cp -= 0x10000;
                    uint16_t high = 0xD800 + static_cast<uint16_t>(cp >> 10);
                    uint16_t low  = 0xDC00 + static_cast<uint16_t>(cp & 0x3FF);
                    result.push_back(static_cast<uint8_t>(high & 0xFF));
                    result.push_back(static_cast<uint8_t>((high >> 8) & 0xFF));
                    result.push_back(static_cast<uint8_t>(low & 0xFF));
                    result.push_back(static_cast<uint8_t>((low >> 8) & 0xFF));
                }
            }
            break;
        }

        case TextEncoding::UTF16_BE: {
            result.push_back(0xFE);
            result.push_back(0xFF);
            size_t i = 0;
            while (i < utf8.size()) {
                uint32_t cp = 0;
                uint8_t b = static_cast<uint8_t>(utf8[i]);
                if (b < 0x80) { cp = b; i++; }
                else if ((b & 0xE0) == 0xC0) {
                    cp = (b & 0x1F) << 6;
                    if (i + 1 < utf8.size()) cp |= (static_cast<uint8_t>(utf8[i + 1]) & 0x3F);
                    i += 2;
                } else if ((b & 0xF0) == 0xE0) {
                    cp = (b & 0x0F) << 12;
                    if (i + 1 < utf8.size()) cp |= (static_cast<uint8_t>(utf8[i + 1]) & 0x3F) << 6;
                    if (i + 2 < utf8.size()) cp |= (static_cast<uint8_t>(utf8[i + 2]) & 0x3F);
                    i += 3;
                } else if ((b & 0xF8) == 0xF0) {
                    cp = (b & 0x07) << 18;
                    if (i + 1 < utf8.size()) cp |= (static_cast<uint8_t>(utf8[i + 1]) & 0x3F) << 12;
                    if (i + 2 < utf8.size()) cp |= (static_cast<uint8_t>(utf8[i + 2]) & 0x3F) << 6;
                    if (i + 3 < utf8.size()) cp |= (static_cast<uint8_t>(utf8[i + 3]) & 0x3F);
                    i += 4;
                } else { i++; continue; }

                if (cp < 0x10000) {
                    result.push_back(static_cast<uint8_t>((cp >> 8) & 0xFF));
                    result.push_back(static_cast<uint8_t>(cp & 0xFF));
                } else {
                    cp -= 0x10000;
                    uint16_t high = 0xD800 + static_cast<uint16_t>(cp >> 10);
                    uint16_t low  = 0xDC00 + static_cast<uint16_t>(cp & 0x3FF);
                    result.push_back(static_cast<uint8_t>((high >> 8) & 0xFF));
                    result.push_back(static_cast<uint8_t>(high & 0xFF));
                    result.push_back(static_cast<uint8_t>((low >> 8) & 0xFF));
                    result.push_back(static_cast<uint8_t>(low & 0xFF));
                }
            }
            break;
        }

        default:
            result.assign(utf8.begin(), utf8.end());
            break;
    }

    return result;
}

// Get human-readable encoding name
// Insan tarafindan okunabilir kodlama adini al
std::string EncodingDetector::encodingName(TextEncoding enc) {
    switch (enc) {
        case TextEncoding::UTF8:      return "utf-8";
        case TextEncoding::UTF8_BOM:  return "utf-8-bom";
        case TextEncoding::UTF16_LE:  return "utf-16le";
        case TextEncoding::UTF16_BE:  return "utf-16be";
        case TextEncoding::UTF32_LE:  return "utf-32le";
        case TextEncoding::UTF32_BE:  return "utf-32be";
        case TextEncoding::ASCII:     return "ascii";
        case TextEncoding::Latin1:    return "latin1";
        default:                      return "unknown";
    }
}

// Parse encoding name string to enum
// Kodlama adi dizesini enum'a ayristir
TextEncoding EncodingDetector::parseEncoding(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "utf-8" || lower == "utf8")             return TextEncoding::UTF8;
    if (lower == "utf-8-bom" || lower == "utf8bom")      return TextEncoding::UTF8_BOM;
    if (lower == "utf-16le" || lower == "utf16le")        return TextEncoding::UTF16_LE;
    if (lower == "utf-16be" || lower == "utf16be")        return TextEncoding::UTF16_BE;
    if (lower == "utf-32le" || lower == "utf32le")        return TextEncoding::UTF32_LE;
    if (lower == "utf-32be" || lower == "utf32be")        return TextEncoding::UTF32_BE;
    if (lower == "ascii" || lower == "us-ascii")          return TextEncoding::ASCII;
    if (lower == "latin1" || lower == "iso-8859-1" || lower == "iso88591")
        return TextEncoding::Latin1;

    return TextEncoding::Unknown;
}
