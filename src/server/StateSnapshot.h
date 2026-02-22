// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include "nlohmann/json.hpp"

class Buffers;

using json = nlohmann::json;

// Utility class for serializing editor state to JSON.
// Editor durumunu JSON'a serializlemek icin yardimci sinif.
// Used by both REST API and WebSocket server for state queries and sync.
// Hem REST API hem de WebSocket sunucusu tarafindan durum sorgulari ve senkronizasyon icin kullanilir.
class StateSnapshot {
public:
    // Complete editor state: cursor, buffer lines, mode, open buffers list
    // Tam editor durumu: imlec, buffer satirlari, mod, acik buffer'lar listesi
    static json fullState(Buffers& buffers);

    // Active buffer content as array of lines
    // Aktif buffer icerigi satir dizisi olarak
    static json activeBuffer(Buffers& buffers);

    // Single line content from the active buffer
    // Aktif buffer'dan tek satir icerigi
    static json bufferLine(Buffers& buffers, int lineNum);

    // Current cursor position {line, col}
    // Mevcut imlec konumu {satir, sutun}
    static json cursorPosition(Buffers& buffers);

    // List of all open buffers with titles and active flag
    // Basliklar ve aktif bayragi ile tum acik buffer'larin listesi
    static json bufferList(Buffers& buffers);
};
