// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Supported text encodings for detection and conversion
// Algilama ve donusturme icin desteklenen metin kodlamalari
enum class TextEncoding {
    UTF8,          // UTF-8 (with or without BOM)
    UTF8_BOM,      // UTF-8 with BOM
    UTF16_LE,      // UTF-16 Little Endian
    UTF16_BE,      // UTF-16 Big Endian
    UTF32_LE,      // UTF-32 Little Endian
    UTF32_BE,      // UTF-32 Big Endian
    ASCII,         // Pure 7-bit ASCII
    Latin1,        // ISO-8859-1 / Latin-1
    Unknown        // Could not determine encoding / Kodlama belirlenemedi
};

// Result of encoding detection
// Kodlama algilama sonucu
struct EncodingResult {
    TextEncoding encoding = TextEncoding::Unknown;  // Detected encoding / Algilanan kodlama
    bool hasBOM = false;                            // Whether a BOM was found / BOM bulunup bulunmadigi
    int bomSize = 0;                                // Size of BOM in bytes / BOM boyutu (bayt)
    double confidence = 0.0;                        // Detection confidence 0.0-1.0 / Algilama guveni 0.0-1.0
};

// Detects file encoding and converts text to/from UTF-8.
// Dosya kodlamasini algilar ve metni UTF-8'e/UTF-8'den donusturur.
// Supports BOM detection, UTF-8 validation, and conversion from common encodings.
// BOM algilama, UTF-8 dogrulama ve yaygin kodlamalardan donusturme destekler.
// No external dependencies (no ICU) - handles UTF-8, UTF-16, UTF-32, Latin-1, ASCII.
// Dis bagimliliklari yok (ICU yok) - UTF-8, UTF-16, UTF-32, Latin-1, ASCII isler.
class EncodingDetector {
public:
    // Detect encoding from raw file bytes
    // Ham dosya baytlarindan kodlamayi algila
    static EncodingResult detect(const std::vector<uint8_t>& data);

    // Detect encoding from raw byte pointer and size
    // Ham bayt isaretcisi ve boyuttan kodlamayi algila
    static EncodingResult detect(const uint8_t* data, size_t size);

    // Detect encoding of a file on disk
    // Diskteki bir dosyanin kodlamasini algila
    static EncodingResult detectFile(const std::string& filePath);

    // Convert raw bytes to UTF-8 string using detected or specified encoding
    // Ham baytlari algilanan veya belirtilen kodlamayi kullanarak UTF-8 dizesine donustur
    static std::string toUTF8(const std::vector<uint8_t>& data, TextEncoding encoding);

    // Convert UTF-8 string to target encoding bytes
    // UTF-8 dizesini hedef kodlama baytlarina donustur
    static std::vector<uint8_t> fromUTF8(const std::string& utf8, TextEncoding encoding);

    // Check if data is valid UTF-8
    // Verinin gecerli UTF-8 olup olmadigini kontrol et
    static bool isValidUTF8(const uint8_t* data, size_t size);

    // Check if data is pure 7-bit ASCII
    // Verinin saf 7-bit ASCII olup olmadigini kontrol et
    static bool isASCII(const uint8_t* data, size_t size);

    // Get human-readable encoding name
    // Insan tarafindan okunabilir kodlama adini al
    static std::string encodingName(TextEncoding enc);

    // Parse encoding name string to enum
    // Kodlama adi dizesini enum'a ayristir
    static TextEncoding parseEncoding(const std::string& name);

private:
    // Check for BOM at the start of data
    // Verinin basindaki BOM'u kontrol et
    static EncodingResult detectBOM(const uint8_t* data, size_t size);

    // Heuristic detection when no BOM is present
    // BOM olmadigi zaman bulussel algilama
    static EncodingResult detectHeuristic(const uint8_t* data, size_t size);

    // UTF-16 to UTF-8 conversion helpers
    // UTF-16'dan UTF-8'e donusturme yardimcilari
    static std::string utf16ToUTF8(const uint8_t* data, size_t size, bool littleEndian);

    // UTF-32 to UTF-8 conversion helpers
    // UTF-32'den UTF-8'e donusturme yardimcilari
    static std::string utf32ToUTF8(const uint8_t* data, size_t size, bool littleEndian);

    // Latin-1 to UTF-8 conversion
    // Latin-1'den UTF-8'e donusturme
    static std::string latin1ToUTF8(const uint8_t* data, size_t size);

    // Encode a single Unicode code point to UTF-8 bytes
    // Tek bir Unicode kod noktasini UTF-8 baytlarina kodla
    static void encodeUTF8(uint32_t codepoint, std::string& out);
};
