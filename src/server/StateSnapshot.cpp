// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "StateSnapshot.h"
#include "buffers.h"

// Capture the full editor state: cursor, buffer content, mode, and open buffer list
// Tam editor durumunu yakala: imlec, tampon icerigi, mod ve acik tampon listesi
json StateSnapshot::fullState(Buffers& buffers) {
    auto& st = buffers.active();
    auto& buf = st.getBuffer();
    auto& cur = st.getCursor();

    json lines = json::array();
    for (int i = 0; i < buf.lineCount(); ++i) {
        lines.push_back(buf.getLine(i));
    }

    EditMode mode = st.getMode();
    std::string modeStr = "normal";
    if (mode == EditMode::Insert) modeStr = "insert";
    else if (mode == EditMode::Visual) modeStr = "visual";

    json bufList = json::array();
    for (size_t i = 0; i < buffers.count(); ++i) {
        bufList.push_back({
            {"index", (int)i},
            {"title", buffers.titleOf(i)},
            {"active", i == buffers.activeIndex()}
        });
    }

    return {
        {"cursor", {{"line", cur.getLine()}, {"col", cur.getCol()}}},
        {"buffer", {
            {"lines", lines},
            {"lineCount", buf.lineCount()},
            {"filePath", st.getFilePath()},
            {"modified", st.isModified()}
        }},
        {"mode", modeStr},
        {"activeIndex", (int)buffers.activeIndex()},
        {"buffers", bufList}
    };
}

// Return the active buffer's content, line count, file path, and modified status
// Aktif tamponun icerigini, satir sayisini, dosya yolunu ve degisiklik durumunu dondur
json StateSnapshot::activeBuffer(Buffers& buffers) {
    auto& buf = buffers.active().getBuffer();
    json lines = json::array();
    for (int i = 0; i < buf.lineCount(); ++i) {
        lines.push_back(buf.getLine(i));
    }
    return {
        {"lines", lines},
        {"lineCount", buf.lineCount()},
        {"filePath", buffers.active().getFilePath()},
        {"modified", buffers.active().isModified()}
    };
}

// Return a single line's content from the active buffer by line number
// Aktif tampondan satir numarasina gore tek bir satirin icerigini dondur
json StateSnapshot::bufferLine(Buffers& buffers, int lineNum) {
    auto& buf = buffers.active().getBuffer();
    if (lineNum < 0 || lineNum >= buf.lineCount()) {
        return {{"error", "line out of range"}, {"line", lineNum}};
    }
    return {{"line", lineNum}, {"content", buf.getLine(lineNum)}};
}

// Return the current cursor position (line and column) in the active buffer
// Aktif tampondaki mevcut imlec konumunu (satir ve sutun) dondur
json StateSnapshot::cursorPosition(Buffers& buffers) {
    auto& cur = buffers.active().getCursor();
    return {{"line", cur.getLine()}, {"col", cur.getCol()}};
}

// Return a list of all open buffers with their index, title, and active status
// Tum acik tamponlarin dizin, baslik ve aktiflik durumuyla listesini dondur
json StateSnapshot::bufferList(Buffers& buffers) {
    json list = json::array();
    for (size_t i = 0; i < buffers.count(); ++i) {
        list.push_back({
            {"index", (int)i},
            {"title", buffers.titleOf(i)},
            {"active", i == buffers.activeIndex()}
        });
    }
    return list;
}
