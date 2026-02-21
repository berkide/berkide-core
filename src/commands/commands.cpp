#include "commands.h"
#include "EditorContext.h"
#include "buffers.h"
#include "EventBus.h"
#include "RegisterManager.h"
#include "Selection.h"
#include "SearchEngine.h"
#include "MarkManager.h"
#include "MacroRecorder.h"
#include "KeymapManager.h"
#include "FoldManager.h"
#include "MultiCursor.h"
#include "WindowManager.h"
#include "SessionManager.h"
#include "IndentEngine.h"
#include "WorkerManager.h"
#include "DiffEngine.h"
#include "CompletionEngine.h"
#include "CharClassifier.h"
#include "EncodingDetector.h"
#include "ProcessManager.h"
#include "TreeSitterEngine.h"
#include "PluginManager.h"
#include "Extmark.h"
#include "BufferOptions.h"
#include "AutoSave.h"
#include "HelpSystem.h"
#include "Logger.h"
#include <sstream>

// Register all core built-in commands (~20 native commands) with the router
// Tum temel yerlesik komutlari (~20 native komut) yonlendiriciyle kaydet
void RegisterCommands(CommandRouter& router, EditorContext* ctx) {

    // --- input.key: Handle special key presses (arrows, Enter, Backspace, etc.) ---
    // --- input.key: Ozel tus basilarini isle (oklar, Enter, Backspace, vb.) ---
    router.registerNative("input.key", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        std::string key = args.value("key", "");
        if (key.empty()) return;

        auto& st  = ctx->buffers->active();
        auto& buf = st.getBuffer();
        auto& cur = st.getCursor();

        if (key == "ArrowUp" || key == "Up") {
            cur.moveUp(buf);
        } else if (key == "ArrowDown" || key == "Down") {
            cur.moveDown(buf);
        } else if (key == "ArrowLeft" || key == "Left") {
            cur.moveLeft(buf);
        } else if (key == "ArrowRight" || key == "Right") {
            cur.moveRight(buf);
        } else if (key == "Home") {
            cur.moveToLineStart();
        } else if (key == "End") {
            cur.moveToLineEnd(buf);
        } else if (key == "Enter") {
            buf.splitLine(cur.getLine(), cur.getCol());
            cur.setPosition(cur.getLine() + 1, 0);
            st.markModified(true);
            if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", st.getFilePath());
        } else if (key == "Backspace") {
            int line = cur.getLine();
            int col  = cur.getCol();
            if (col > 0) {
                buf.deleteChar(line, col - 1);
                cur.setPosition(line, col - 1);
            } else if (line > 0) {
                int prevLen = buf.columnCount(line - 1);
                buf.joinLines(line - 1, line);
                cur.setPosition(line - 1, prevLen);
            }
            st.markModified(true);
            if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", st.getFilePath());
        } else if (key == "Delete") {
            int line = cur.getLine();
            int col  = cur.getCol();
            if (col < buf.columnCount(line)) {
                buf.deleteChar(line, col);
            } else if (line + 1 < buf.lineCount()) {
                buf.joinLines(line, line + 1);
            }
            st.markModified(true);
            if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", st.getFilePath());
        } else if (key == "PageUp") {
            for (int i = 0; i < 20; ++i) cur.moveUp(buf);
        } else if (key == "PageDown") {
            for (int i = 0; i < 20; ++i) cur.moveDown(buf);
        } else if (key == "Ctrl+S" || key == "C-s") {
            ctx->buffers->saveActive();
            if (ctx->eventBus) ctx->eventBus->emit("fileSaved", st.getFilePath());
        } else {
            LOG_DEBUG("[Command] input.key unhandled: ", key);
            return;
        }

        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });

    // --- input.char: Insert character(s) at cursor ---
    // --- input.char: Imlec konumuna karakter(ler) ekle ---
    router.registerNative("input.char", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        std::string text = args.value("text", "");
        if (text.empty()) return;

        auto& st  = ctx->buffers->active();
        auto& buf = st.getBuffer();
        auto& cur = st.getCursor();

        buf.insertText(cur.getLine(), cur.getCol(), text);
        // Advance cursor by the length of inserted text
        // Eklenen metnin uzunlugu kadar imleci ilerlet
        for (size_t i = 0; i < text.size(); ++i) {
            cur.moveRight(buf);
        }

        st.markModified(true);
        if (ctx->eventBus) {
            ctx->eventBus->emit("bufferChanged", st.getFilePath());
            ctx->eventBus->emit("cursorMoved");
        }
    });

    // --- cursor.up/down/left/right/home/end ---
    // --- cursor.yukari/asagi/sol/sag/bas/son ---
    router.registerNative("cursor.up", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        auto& st = ctx->buffers->active();
        st.getCursor().moveUp(st.getBuffer());
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });
    router.registerNative("cursor.down", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        auto& st = ctx->buffers->active();
        st.getCursor().moveDown(st.getBuffer());
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });
    router.registerNative("cursor.left", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        auto& st = ctx->buffers->active();
        st.getCursor().moveLeft(st.getBuffer());
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });
    router.registerNative("cursor.right", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        auto& st = ctx->buffers->active();
        st.getCursor().moveRight(st.getBuffer());
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });
    router.registerNative("cursor.home", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        ctx->buffers->active().getCursor().moveToLineStart();
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });
    router.registerNative("cursor.end", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        auto& st = ctx->buffers->active();
        st.getCursor().moveToLineEnd(st.getBuffer());
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });
    router.registerNative("cursor.setPosition", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        int line = args.value("line", 0);
        int col  = args.value("col", 0);
        ctx->buffers->active().getCursor().setPosition(line, col);
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });

    // --- buffer.insert / buffer.delete / buffer.splitLine ---
    // --- buffer.ekle / buffer.sil / buffer.satirBol ---
    router.registerNative("buffer.insert", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        std::string text = args.value("text", "");
        int line = args.value("line", -1);
        int col  = args.value("col", -1);
        auto& st  = ctx->buffers->active();
        auto& buf = st.getBuffer();
        auto& cur = st.getCursor();
        if (line < 0) line = cur.getLine();
        if (col < 0)  col  = cur.getCol();
        buf.insertText(line, col, text);
        st.markModified(true);
        if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", st.getFilePath());
    });
    router.registerNative("buffer.delete", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        int line = args.value("line", -1);
        int col  = args.value("col", -1);
        auto& st  = ctx->buffers->active();
        auto& buf = st.getBuffer();
        auto& cur = st.getCursor();
        if (line < 0) line = cur.getLine();
        if (col < 0)  col  = cur.getCol();
        buf.deleteChar(line, col);
        st.markModified(true);
        if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", st.getFilePath());
    });
    router.registerNative("buffer.splitLine", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        int line = args.value("line", -1);
        int col  = args.value("col", -1);
        auto& st  = ctx->buffers->active();
        auto& buf = st.getBuffer();
        auto& cur = st.getCursor();
        if (line < 0) line = cur.getLine();
        if (col < 0)  col  = cur.getCol();
        buf.splitLine(line, col);
        st.markModified(true);
        if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", st.getFilePath());
    });
    router.registerNative("buffer.new", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        std::string name = args.value("name", "untitled");
        ctx->buffers->newDocument(name);
        LOG_INFO("[Command] buffer.new: ", name);
        if (ctx->eventBus) ctx->eventBus->emit("tabChanged");
    });

    // --- edit.undo / edit.redo ---
    // --- edit.geriAl / edit.yinele ---
    router.registerNative("edit.undo", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        auto& st = ctx->buffers->active();
        st.getUndo().undo(st.getBuffer());
        st.syncCursor();
        if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", st.getFilePath());
    });
    router.registerNative("edit.redo", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        auto& st = ctx->buffers->active();
        st.getUndo().redo(st.getBuffer());
        st.syncCursor();
        if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", st.getFilePath());
    });

    // --- file.open / file.save / file.saveAs ---
    // --- dosya.ac / dosya.kaydet / dosya.farkliKaydet ---
    router.registerNative("file.open", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        std::string path = args.value("path", "");
        if (path.empty()) return;
        ctx->buffers->openFile(path);
        if (ctx->eventBus) ctx->eventBus->emit("tabChanged");
    });
    router.registerNative("file.save", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        ctx->buffers->saveActive();
        auto& st = ctx->buffers->active();
        if (ctx->eventBus) ctx->eventBus->emit("fileSaved", st.getFilePath());
    });
    router.registerNative("file.saveAs", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        std::string path = args.value("path", "");
        if (path.empty()) return;
        auto& st = ctx->buffers->active();
        st.setFilePath(path);
        ctx->buffers->saveActive();
        if (ctx->eventBus) ctx->eventBus->emit("fileSaved", path);
    });

    // --- tab.next / tab.prev / tab.close / tab.switchTo ---
    // --- sekme.sonraki / sekme.onceki / sekme.kapat / sekme.gec ---
    router.registerNative("tab.next", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        ctx->buffers->next();
        if (ctx->eventBus) ctx->eventBus->emit("tabChanged");
    });
    router.registerNative("tab.prev", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        ctx->buffers->prev();
        if (ctx->eventBus) ctx->eventBus->emit("tabChanged");
    });
    router.registerNative("tab.close", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        ctx->buffers->closeActive();
        if (ctx->eventBus) ctx->eventBus->emit("tabChanged");
    });
    router.registerNative("tab.switchTo", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        int index = args.value("index", -1);
        if (index < 0) return;
        ctx->buffers->setActive(static_cast<size_t>(index));
        if (ctx->eventBus) ctx->eventBus->emit("tabChanged");
    });

    // --- mode.set: Change editing mode with selection integration ---
    // --- mod.ayarla: Secim entegrasyonuyla duzenleme modunu degistir ---
    router.registerNative("mode.set", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        std::string mode = args.value("mode", "normal");
        auto& st = ctx->buffers->active();

        if (mode == "insert") {
            st.getSelection().clear();
            st.setMode(EditMode::Insert);
        } else if (mode == "visual") {
            // Enter Visual mode: set anchor at current cursor position
            // Visual moda gir: baglama noktasini mevcut imlec konumuna ayarla
            auto& cur = st.getCursor();
            st.getSelection().setAnchor(cur.getLine(), cur.getCol());
            st.setMode(EditMode::Visual);
            if (ctx->eventBus) ctx->eventBus->emit("selectionChanged");
        } else if (mode == "visual-line") {
            // Line-wise Visual mode
            // Satir bazli Visual modu
            auto& cur = st.getCursor();
            auto& sel = st.getSelection();
            sel.setAnchor(cur.getLine(), 0);
            sel.setType(SelectionType::Line);
            st.setMode(EditMode::Visual);
            if (ctx->eventBus) ctx->eventBus->emit("selectionChanged");
        } else if (mode == "visual-block") {
            // Block (column) Visual mode
            // Blok (sutun) Visual modu
            auto& cur = st.getCursor();
            auto& sel = st.getSelection();
            sel.setAnchor(cur.getLine(), cur.getCol());
            sel.setType(SelectionType::Block);
            st.setMode(EditMode::Visual);
            if (ctx->eventBus) ctx->eventBus->emit("selectionChanged");
        } else {
            // Normal mode: clear any active selection
            // Normal mod: aktif secimi temizle
            st.getSelection().clear();
            st.setMode(EditMode::Normal);
            if (ctx->eventBus) ctx->eventBus->emit("selectionChanged");
        }
        if (ctx->eventBus) ctx->eventBus->emit("modeChanged", mode);
    });

    // --- selection.selectAll: Select all text in the buffer ---
    // --- selection.tumunuSec: Buffer'daki tum metni sec ---
    router.registerNative("selection.selectAll", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        auto& st  = ctx->buffers->active();
        auto& buf = st.getBuffer();
        auto& sel = st.getSelection();
        auto& cur = st.getCursor();

        sel.setAnchor(0, 0);
        sel.setType(SelectionType::Char);
        int lastLine = buf.lineCount() - 1;
        cur.setPosition(lastLine, buf.columnCount(lastLine));
        st.setMode(EditMode::Visual);
        if (ctx->eventBus) {
            ctx->eventBus->emit("selectionChanged");
            ctx->eventBus->emit("modeChanged", "visual");
        }
    });

    // --- edit.yank: Copy selected text to register ---
    // --- edit.kopyala: Secili metni register'a kopyala ---
    router.registerNative("edit.yank", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        auto& st  = ctx->buffers->active();
        auto& sel = st.getSelection();
        auto& cur = st.getCursor();

        if (!sel.isActive()) return;

        bool linewise = (sel.type() == SelectionType::Line);
        std::string text = linewise
            ? sel.getLineText(st.getBuffer(), cur.getLine(), cur.getCol())
            : sel.getText(st.getBuffer(), cur.getLine(), cur.getCol());

        std::string regName = args.value("register", "");
        if (ctx->registers) {
            if (!regName.empty()) {
                ctx->registers->set(regName, text, linewise);
            }
            ctx->registers->recordYank(text, linewise);
        }

        // Exit Visual mode after yank
        // Kopyalamadan sonra Visual moddan cik
        sel.clear();
        st.setMode(EditMode::Normal);
        if (ctx->eventBus) {
            ctx->eventBus->emit("selectionChanged");
            ctx->eventBus->emit("modeChanged", "normal");
        }
    });

    // --- edit.paste: Paste from register at cursor position ---
    // --- edit.yapistir: Register'dan imlec konumuna yapistir ---
    router.registerNative("edit.paste", [ctx](const json& args) {
        if (!ctx || !ctx->buffers || !ctx->registers) return;
        auto& st  = ctx->buffers->active();
        auto& buf = st.getBuffer();
        auto& cur = st.getCursor();

        std::string regName = args.value("register", "\"");
        RegisterEntry entry = ctx->registers->get(regName);
        if (entry.content.empty()) return;

        if (entry.linewise) {
            // Line-wise paste: insert below current line
            // Satir bazli yapistir: mevcut satirin altina ekle
            int targetLine = cur.getLine() + 1;
            std::string content = entry.content;
            // Split content into lines and insert each
            // Icerigi satirlara bol ve her birini ekle
            size_t pos = 0;
            while (pos < content.size()) {
                size_t nl = content.find('\n', pos);
                std::string line = (nl == std::string::npos)
                    ? content.substr(pos) : content.substr(pos, nl - pos);
                buf.insertLineAt(targetLine, line);
                ++targetLine;
                pos = (nl == std::string::npos) ? content.size() : nl + 1;
            }
            cur.setPosition(cur.getLine() + 1, 0);
        } else {
            // Character-wise paste: insert at cursor
            // Karakter bazli yapistir: imlec konumuna ekle
            buf.insertText(cur.getLine(), cur.getCol(), entry.content);
            // Move cursor to end of pasted text
            // Imleci yapistirilan metnin sonuna tasi
            int line = cur.getLine();
            int col  = cur.getCol();
            for (char c : entry.content) {
                if (c == '\n') {
                    ++line;
                    col = 0;
                } else {
                    ++col;
                }
            }
            cur.setPosition(line, col);
        }

        st.markModified(true);
        if (ctx->eventBus) {
            ctx->eventBus->emit("bufferChanged", st.getFilePath());
            ctx->eventBus->emit("cursorMoved");
        }
    });

    // --- edit.cut: Cut selected text (yank + delete) ---
    // --- edit.kes: Secili metni kes (kopyala + sil) ---
    router.registerNative("edit.cut", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        auto& st  = ctx->buffers->active();
        auto& buf = st.getBuffer();
        auto& sel = st.getSelection();
        auto& cur = st.getCursor();

        if (!sel.isActive()) return;

        bool linewise = (sel.type() == SelectionType::Line);
        std::string text = linewise
            ? sel.getLineText(buf, cur.getLine(), cur.getCol())
            : sel.getText(buf, cur.getLine(), cur.getCol());

        // Store in register before deleting
        // Silmeden once register'a kaydet
        std::string regName = args.value("register", "");
        if (ctx->registers) {
            if (!regName.empty()) {
                ctx->registers->set(regName, text, linewise);
            }
            ctx->registers->recordDelete(text, linewise);
        }

        // Delete selected range
        // Secili araligisil
        int sLine, sCol, eLine, eCol;
        sel.getRange(cur.getLine(), cur.getCol(), sLine, sCol, eLine, eCol);

        if (linewise) {
            // Delete whole lines
            // Tum satirlari sil
            for (int i = eLine; i >= sLine; --i) {
                buf.deleteLine(i);
            }
            if (buf.lineCount() == 0) {
                buf.insertLine("");
            }
            cur.setPosition(std::min(sLine, buf.lineCount() - 1), 0);
        } else {
            buf.deleteRange(sLine, sCol, eLine, eCol);
            cur.setPosition(sLine, sCol);
        }

        sel.clear();
        st.setMode(EditMode::Normal);
        st.markModified(true);
        if (ctx->eventBus) {
            ctx->eventBus->emit("bufferChanged", st.getFilePath());
            ctx->eventBus->emit("selectionChanged");
            ctx->eventBus->emit("cursorMoved");
            ctx->eventBus->emit("modeChanged", "normal");
        }
    });

    // --- edit.deleteLine: Delete current line, store in register ---
    // --- edit.satirSil: Mevcut satiri sil, register'a kaydet ---
    router.registerNative("edit.deleteLine", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        auto& st  = ctx->buffers->active();
        auto& buf = st.getBuffer();
        auto& cur = st.getCursor();

        int line = cur.getLine();
        if (line >= buf.lineCount()) return;

        std::string text = buf.getLine(line) + "\n";

        if (ctx->registers) {
            std::string regName = args.value("register", "");
            if (!regName.empty()) {
                ctx->registers->set(regName, text, true);
            }
            ctx->registers->recordDelete(text, true);
        }

        buf.deleteLine(line);
        if (buf.lineCount() == 0) {
            buf.insertLine("");
        }
        cur.setPosition(std::min(line, buf.lineCount() - 1), 0);

        st.markModified(true);
        if (ctx->eventBus) {
            ctx->eventBus->emit("bufferChanged", st.getFilePath());
            ctx->eventBus->emit("cursorMoved");
        }
    });

    // --- search.forward: Find next occurrence of pattern ---
    // --- search.ileri: Kalibin sonraki olumunu bul ---
    router.registerNative("search.forward", [ctx](const json& args) {
        if (!ctx || !ctx->buffers || !ctx->searchEngine) return;
        std::string pattern = args.value("pattern", "");
        if (pattern.empty()) return;

        SearchOptions opts;
        opts.caseSensitive = args.value("caseSensitive", true);
        opts.regex         = args.value("regex", false);
        opts.wholeWord     = args.value("wholeWord", false);
        opts.wrapAround    = args.value("wrapAround", true);

        auto& st  = ctx->buffers->active();
        auto& cur = st.getCursor();

        ctx->searchEngine->setLastPattern(pattern);
        ctx->searchEngine->setLastOptions(opts);

        SearchMatch match;
        if (ctx->searchEngine->findForward(st.getBuffer(), pattern,
                                            cur.getLine(), cur.getCol() + 1, match, opts)) {
            cur.setPosition(match.line, match.col);
            if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
        }
    });

    // --- search.backward: Find previous occurrence of pattern ---
    // --- search.geri: Kalibin onceki olumunu bul ---
    router.registerNative("search.backward", [ctx](const json& args) {
        if (!ctx || !ctx->buffers || !ctx->searchEngine) return;
        std::string pattern = args.value("pattern", "");
        if (pattern.empty()) return;

        SearchOptions opts;
        opts.caseSensitive = args.value("caseSensitive", true);
        opts.regex         = args.value("regex", false);
        opts.wholeWord     = args.value("wholeWord", false);
        opts.wrapAround    = args.value("wrapAround", true);

        auto& st  = ctx->buffers->active();
        auto& cur = st.getCursor();

        ctx->searchEngine->setLastPattern(pattern);
        ctx->searchEngine->setLastOptions(opts);

        SearchMatch match;
        if (ctx->searchEngine->findBackward(st.getBuffer(), pattern,
                                             cur.getLine(), cur.getCol(), match, opts)) {
            cur.setPosition(match.line, match.col);
            if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
        }
    });

    // --- search.next: Repeat last search forward ---
    // --- search.sonraki: Son aramayi ileri tekrarla ---
    router.registerNative("search.next", [ctx](const json&) {
        if (!ctx || !ctx->buffers || !ctx->searchEngine) return;
        const std::string& pattern = ctx->searchEngine->lastPattern();
        if (pattern.empty()) return;

        auto& st  = ctx->buffers->active();
        auto& cur = st.getCursor();
        SearchMatch match;
        if (ctx->searchEngine->findForward(st.getBuffer(), pattern,
                                            cur.getLine(), cur.getCol() + 1,
                                            match, ctx->searchEngine->lastOptions())) {
            cur.setPosition(match.line, match.col);
            if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
        }
    });

    // --- search.prev: Repeat last search backward ---
    // --- search.onceki: Son aramayi geri tekrarla ---
    router.registerNative("search.prev", [ctx](const json&) {
        if (!ctx || !ctx->buffers || !ctx->searchEngine) return;
        const std::string& pattern = ctx->searchEngine->lastPattern();
        if (pattern.empty()) return;

        auto& st  = ctx->buffers->active();
        auto& cur = st.getCursor();
        SearchMatch match;
        if (ctx->searchEngine->findBackward(st.getBuffer(), pattern,
                                             cur.getLine(), cur.getCol(),
                                             match, ctx->searchEngine->lastOptions())) {
            cur.setPosition(match.line, match.col);
            if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
        }
    });

    // --- search.replace: Replace next occurrence ---
    // --- search.degistir: Sonraki olumu degistir ---
    router.registerNative("search.replace", [ctx](const json& args) {
        if (!ctx || !ctx->buffers || !ctx->searchEngine) return;
        std::string pattern     = args.value("pattern", "");
        std::string replacement = args.value("replacement", "");
        if (pattern.empty()) return;

        SearchOptions opts;
        opts.caseSensitive = args.value("caseSensitive", true);
        opts.regex         = args.value("regex", false);

        auto& st  = ctx->buffers->active();
        auto& cur = st.getCursor();
        SearchMatch nextMatch;
        if (ctx->searchEngine->replaceNext(st.getBuffer(), pattern, replacement,
                                            cur.getLine(), cur.getCol(), nextMatch, opts)) {
            st.markModified(true);
            cur.setPosition(nextMatch.line, nextMatch.col);
            if (ctx->eventBus) {
                ctx->eventBus->emit("bufferChanged", st.getFilePath());
                ctx->eventBus->emit("cursorMoved");
            }
        }
    });

    // --- search.replaceAll: Replace all occurrences ---
    // --- search.tumunuDegistir: Tum oluslari degistir ---
    router.registerNative("search.replaceAll", [ctx](const json& args) {
        if (!ctx || !ctx->buffers || !ctx->searchEngine) return;
        std::string pattern     = args.value("pattern", "");
        std::string replacement = args.value("replacement", "");
        if (pattern.empty()) return;

        SearchOptions opts;
        opts.caseSensitive = args.value("caseSensitive", true);
        opts.regex         = args.value("regex", false);

        auto& st = ctx->buffers->active();
        int count = ctx->searchEngine->replaceAll(st.getBuffer(), pattern, replacement, opts);
        if (count > 0) {
            st.markModified(true);
            if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", st.getFilePath());
            LOG_INFO("[Search] Replaced ", count, " occurrences");
        }
    });

    // --- mark.set: Set a named mark at cursor position ---
    // --- mark.ayarla: Imlec konumunda adlandirilmis isaret ayarla ---
    router.registerNative("mark.set", [ctx](const json& args) {
        if (!ctx || !ctx->buffers || !ctx->markManager) return;
        std::string name = args.value("name", "");
        if (name.empty()) return;
        auto& st  = ctx->buffers->active();
        auto& cur = st.getCursor();
        ctx->markManager->set(name, cur.getLine(), cur.getCol(), st.getFilePath());
    });

    // --- mark.jump: Jump to a named mark ---
    // --- mark.atla: Adlandirilmis isaretin konumuna atla ---
    router.registerNative("mark.jump", [ctx](const json& args) {
        if (!ctx || !ctx->buffers || !ctx->markManager) return;
        std::string name = args.value("name", "");
        if (name.empty()) return;

        const Mark* m = ctx->markManager->get(name);
        if (!m) return;

        auto& st  = ctx->buffers->active();
        auto& cur = st.getCursor();

        // Push current position to jump list before jumping
        // Atlamadan once mevcut konumu atlama listesine it
        ctx->markManager->pushJump(st.getFilePath(), cur.getLine(), cur.getCol());
        cur.setPosition(m->line, m->col);
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });

    // --- mark.jumpBack: Navigate backward in jump list ---
    // --- mark.geriAtla: Atlama listesinde geri git ---
    router.registerNative("mark.jumpBack", [ctx](const json&) {
        if (!ctx || !ctx->buffers || !ctx->markManager) return;
        auto& st  = ctx->buffers->active();
        auto& cur = st.getCursor();
        ctx->markManager->pushJump(st.getFilePath(), cur.getLine(), cur.getCol());

        JumpEntry entry;
        if (ctx->markManager->jumpBack(entry)) {
            cur.setPosition(entry.line, entry.col);
            if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
        }
    });

    // --- mark.jumpForward: Navigate forward in jump list ---
    // --- mark.ileriAtla: Atlama listesinde ileri git ---
    router.registerNative("mark.jumpForward", [ctx](const json&) {
        if (!ctx || !ctx->buffers || !ctx->markManager) return;
        JumpEntry entry;
        if (ctx->markManager->jumpForward(entry)) {
            ctx->buffers->active().getCursor().setPosition(entry.line, entry.col);
            if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
        }
    });

    // --- fold.create: Create a fold region ---
    // --- fold.olustur: Bir katlama bolgesi olustur ---
    router.registerNative("fold.create", [ctx](const json& args) {
        if (!ctx || !ctx->foldManager) return;
        int startLine = args.value("startLine", -1);
        int endLine   = args.value("endLine", -1);
        if (startLine < 0 || endLine < 0) return;
        std::string label = args.value("label", "");
        ctx->foldManager->create(startLine, endLine, label);
        if (ctx->eventBus) ctx->eventBus->emit("foldChanged");
    });

    // --- fold.toggle: Toggle fold at cursor line ---
    // --- fold.degistir: Imlec satirindaki katlamayi degistir ---
    router.registerNative("fold.toggle", [ctx](const json& args) {
        if (!ctx || !ctx->foldManager || !ctx->buffers) return;
        int line = args.value("line", -1);
        if (line < 0) line = ctx->buffers->active().getCursor().getLine();
        ctx->foldManager->toggle(line);
        if (ctx->eventBus) ctx->eventBus->emit("foldChanged");
    });

    // --- fold.collapse: Collapse fold at cursor line ---
    // --- fold.kapat: Imlec satirindaki katlamayi kapat ---
    router.registerNative("fold.collapse", [ctx](const json& args) {
        if (!ctx || !ctx->foldManager || !ctx->buffers) return;
        int line = args.value("line", -1);
        if (line < 0) line = ctx->buffers->active().getCursor().getLine();
        ctx->foldManager->collapse(line);
        if (ctx->eventBus) ctx->eventBus->emit("foldChanged");
    });

    // --- fold.expand: Expand fold at cursor line ---
    // --- fold.ac: Imlec satirindaki katlamayi ac ---
    router.registerNative("fold.expand", [ctx](const json& args) {
        if (!ctx || !ctx->foldManager || !ctx->buffers) return;
        int line = args.value("line", -1);
        if (line < 0) line = ctx->buffers->active().getCursor().getLine();
        ctx->foldManager->expand(line);
        if (ctx->eventBus) ctx->eventBus->emit("foldChanged");
    });

    // --- fold.collapseAll: Collapse all folds ---
    // --- fold.hepsiniKapat: Tum katlamalari kapat ---
    router.registerNative("fold.collapseAll", [ctx](const json&) {
        if (!ctx || !ctx->foldManager) return;
        ctx->foldManager->collapseAll();
        if (ctx->eventBus) ctx->eventBus->emit("foldChanged");
    });

    // --- fold.expandAll: Expand all folds ---
    // --- fold.hepsiniAc: Tum katlamalari ac ---
    router.registerNative("fold.expandAll", [ctx](const json&) {
        if (!ctx || !ctx->foldManager) return;
        ctx->foldManager->expandAll();
        if (ctx->eventBus) ctx->eventBus->emit("foldChanged");
    });

    // --- macro.record: Start recording macro into a register ---
    // --- macro.kaydet: Bir register'a makro kaydetmeye basla ---
    router.registerNative("macro.record", [ctx](const json& args) {
        if (!ctx || !ctx->macroRecorder) return;
        std::string reg = args.value("register", "q");
        ctx->macroRecorder->startRecording(reg);
        if (ctx->eventBus) ctx->eventBus->emit("macroRecordingChanged", "started");
    });

    // --- macro.stop: Stop recording macro ---
    // --- macro.durdur: Makro kaydini durdur ---
    router.registerNative("macro.stop", [ctx](const json&) {
        if (!ctx || !ctx->macroRecorder) return;
        ctx->macroRecorder->stopRecording();
        if (ctx->eventBus) ctx->eventBus->emit("macroRecordingChanged", "stopped");
    });

    // --- macro.play: Play back a macro from register ---
    // --- macro.oynat: Register'dan bir makroyu oynat ---
    router.registerNative("macro.play", [ctx](const json& args) {
        if (!ctx || !ctx->macroRecorder || !ctx->commandRouter) return;
        std::string reg = args.value("register", "q");
        int count = args.value("count", 1);
        if (count < 1) count = 1;

        const auto* macro = ctx->macroRecorder->getMacro(reg);
        if (!macro) return;

        for (int i = 0; i < count; ++i) {
            for (auto& cmd : *macro) {
                json cmdArgs = cmd.argsJson.empty() ? json::object() : json::parse(cmd.argsJson, nullptr, false);
                if (cmdArgs.is_discarded()) cmdArgs = json::object();
                ctx->commandRouter->execute(cmd.name, cmdArgs);
            }
        }
    });

    // --- keymap.set: Set a key binding ---
    // --- keymap.ayarla: Tus baglantisi ayarla ---
    router.registerNative("keymap.set", [ctx](const json& args) {
        if (!ctx || !ctx->keymapManager) return;
        std::string keymapName = args.value("keymap", "global");
        std::string keys       = args.value("keys", "");
        std::string command    = args.value("command", "");
        std::string argsJson   = args.value("argsJson", "");
        if (keys.empty() || command.empty()) return;
        ctx->keymapManager->set(keymapName, keys, command, argsJson);
    });

    // --- keymap.remove: Remove a key binding ---
    // --- keymap.kaldir: Tus baglantisini kaldir ---
    router.registerNative("keymap.remove", [ctx](const json& args) {
        if (!ctx || !ctx->keymapManager) return;
        std::string keymapName = args.value("keymap", "global");
        std::string keys       = args.value("keys", "");
        if (keys.empty()) return;
        ctx->keymapManager->remove(keymapName, keys);
    });

    // --- window.splitH: Split active window horizontally ---
    // --- window.yatayBol: Aktif pencereyi yatay bol ---
    router.registerNative("window.splitH", [ctx](const json&) {
        if (!ctx || !ctx->windowManager) return;
        ctx->windowManager->splitActive(SplitDirection::Horizontal);
        if (ctx->eventBus) ctx->eventBus->emit("windowChanged");
    });

    // --- window.splitV: Split active window vertically ---
    // --- window.dikeyBol: Aktif pencereyi dikey bol ---
    router.registerNative("window.splitV", [ctx](const json&) {
        if (!ctx || !ctx->windowManager) return;
        ctx->windowManager->splitActive(SplitDirection::Vertical);
        if (ctx->eventBus) ctx->eventBus->emit("windowChanged");
    });

    // --- window.close: Close active window ---
    // --- window.kapat: Aktif pencereyi kapat ---
    router.registerNative("window.close", [ctx](const json&) {
        if (!ctx || !ctx->windowManager) return;
        ctx->windowManager->closeActive();
        if (ctx->eventBus) ctx->eventBus->emit("windowChanged");
    });

    // --- window.focusNext: Focus next window ---
    // --- window.sonrakiOdak: Sonraki pencereye odaklan ---
    router.registerNative("window.focusNext", [ctx](const json&) {
        if (!ctx || !ctx->windowManager) return;
        ctx->windowManager->focusNext();
        if (ctx->eventBus) ctx->eventBus->emit("windowChanged");
    });

    // --- window.focusPrev: Focus previous window ---
    // --- window.oncekiOdak: Onceki pencereye odaklan ---
    router.registerNative("window.focusPrev", [ctx](const json&) {
        if (!ctx || !ctx->windowManager) return;
        ctx->windowManager->focusPrev();
        if (ctx->eventBus) ctx->eventBus->emit("windowChanged");
    });

    // --- window.equalize: Equalize all split ratios ---
    // --- window.esitle: Tum bolme oranlarini esitle ---
    router.registerNative("window.equalize", [ctx](const json&) {
        if (!ctx || !ctx->windowManager) return;
        ctx->windowManager->equalize();
        if (ctx->eventBus) ctx->eventBus->emit("windowChanged");
    });

    // --- multicursor.addAbove: Add cursor on the line above ---
    // --- multicursor.ustEkle: Ustteki satira imlec ekle ---
    router.registerNative("multicursor.addAbove", [ctx](const json&) {
        if (!ctx || !ctx->multiCursor || !ctx->buffers) return;
        auto& primary = ctx->multiCursor->primary();
        if (primary.line > 0) {
            ctx->multiCursor->addCursor(primary.line - 1, primary.col);
            if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
        }
    });

    // --- multicursor.addBelow: Add cursor on the line below ---
    // --- multicursor.altEkle: Alttaki satira imlec ekle ---
    router.registerNative("multicursor.addBelow", [ctx](const json&) {
        if (!ctx || !ctx->multiCursor || !ctx->buffers) return;
        auto& buf = ctx->buffers->active().getBuffer();
        auto& primary = ctx->multiCursor->primary();
        if (primary.line < buf.lineCount() - 1) {
            ctx->multiCursor->addCursor(primary.line + 1, primary.col);
            if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
        }
    });

    // --- multicursor.addNextMatch: Add cursor at next match of word under cursor ---
    // --- multicursor.sonrakiEsleme: Imlec altindaki kelimenin sonraki olusumuna imlec ekle ---
    router.registerNative("multicursor.addNextMatch", [ctx](const json& args) {
        if (!ctx || !ctx->multiCursor || !ctx->buffers) return;
        std::string word = args.value("word", "");
        if (word.empty()) return;
        ctx->multiCursor->addCursorAtNextMatch(ctx->buffers->active().getBuffer(), word);
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });

    // --- multicursor.clear: Clear all secondary cursors ---
    // --- multicursor.temizle: Tum ikincil imlecleri temizle ---
    router.registerNative("multicursor.clear", [ctx](const json&) {
        if (!ctx || !ctx->multiCursor) return;
        ctx->multiCursor->clearSecondary();
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });

    // --- session.save: Save current session ---
    // --- session.kaydet: Mevcut oturumu kaydet ---
    router.registerNative("session.save", [ctx](const json&) {
        if (!ctx || !ctx->sessionManager || !ctx->buffers) return;
        ctx->sessionManager->save(*ctx->buffers);
    });

    // --- session.saveAs: Save session with a name ---
    // --- session.farklıKaydet: Oturumu adla kaydet ---
    router.registerNative("session.saveAs", [ctx](const json& args) {
        if (!ctx || !ctx->sessionManager || !ctx->buffers) return;
        std::string name = args.value("name", "");
        if (name.empty()) return;
        ctx->sessionManager->saveAs(name, *ctx->buffers);
    });

    // --- session.load: Load default session ---
    // --- session.yukle: Varsayilan oturumu yukle ---
    router.registerNative("session.load", [ctx](const json&) {
        if (!ctx || !ctx->sessionManager || !ctx->buffers) return;
        SessionState state;
        if (ctx->sessionManager->load(state)) {
            for (const auto& doc : state.documents) {
                if (!doc.filePath.empty()) {
                    ctx->buffers->openFile(doc.filePath);
                }
            }
        }
    });

    // --- session.delete: Delete a named session ---
    // --- session.sil: Adlandirilmis oturumu sil ---
    router.registerNative("session.delete", [ctx](const json& args) {
        if (!ctx || !ctx->sessionManager) return;
        std::string name = args.value("name", "");
        if (name.empty()) return;
        ctx->sessionManager->deleteSession(name);
    });

    // --- indent.increase: Increase indent of current line ---
    // --- indent.artir: Mevcut satirin girintisini artir ---
    router.registerNative("indent.increase", [ctx](const json& args) {
        if (!ctx || !ctx->indentEngine || !ctx->buffers) return;
        auto& st  = ctx->buffers->active();
        auto& buf = st.getBuffer();
        int line = args.value("line", -1);
        if (line < 0) line = st.getCursor().getLine();
        if (line < 0 || line >= buf.lineCount()) return;
        buf.getLineRef(line) = ctx->indentEngine->increaseIndent(buf.getLine(line));
        st.markModified(true);
        if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", st.getFilePath());
    });

    // --- indent.decrease: Decrease indent of current line ---
    // --- indent.azalt: Mevcut satirin girintisini azalt ---
    router.registerNative("indent.decrease", [ctx](const json& args) {
        if (!ctx || !ctx->indentEngine || !ctx->buffers) return;
        auto& st  = ctx->buffers->active();
        auto& buf = st.getBuffer();
        int line = args.value("line", -1);
        if (line < 0) line = st.getCursor().getLine();
        if (line < 0 || line >= buf.lineCount()) return;
        buf.getLineRef(line) = ctx->indentEngine->decreaseIndent(buf.getLine(line));
        st.markModified(true);
        if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", st.getFilePath());
    });

    // --- indent.reindent: Reindent a range of lines ---
    // --- indent.yenidenGirintile: Satir araligini yeniden girintile ---
    router.registerNative("indent.reindent", [ctx](const json& args) {
        if (!ctx || !ctx->indentEngine || !ctx->buffers) return;
        auto& st  = ctx->buffers->active();
        auto& buf = st.getBuffer();
        int startLine = args.value("startLine", 0);
        int endLine   = args.value("endLine", buf.lineCount() - 1);
        ctx->indentEngine->reindentRange(buf, startLine, endLine);
        st.markModified(true);
        if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", st.getFilePath());
    });

    // --- worker.create: Create a worker from script file ---
    // --- worker.olustur: Betik dosyasindan calisan olustur ---
    router.registerNative("worker.create", [ctx](const json& args) {
        if (!ctx || !ctx->workerManager) return;
        std::string path = args.value("path", "");
        if (path.empty()) return;
        int id = ctx->workerManager->createWorker(path);
        LOG_INFO("[Command] worker.create: id=", id, " path=", path);
    });

    // --- worker.terminate: Terminate a worker ---
    // --- worker.sonlandir: Bir calisani sonlandir ---
    router.registerNative("worker.terminate", [ctx](const json& args) {
        if (!ctx || !ctx->workerManager) return;
        int id = args.value("id", -1);
        if (id < 0) return;
        ctx->workerManager->terminate(id);
    });

    // --- worker.terminateAll: Terminate all workers ---
    // --- worker.tumunuSonlandir: Tum calisanlari sonlandir ---
    router.registerNative("worker.terminateAll", [ctx](const json&) {
        if (!ctx || !ctx->workerManager) return;
        ctx->workerManager->terminateAll();
    });

    // --- app.quit / app.about ---
    router.registerNative("app.quit", [](const json&) {
        LOG_INFO("[Command] app.quit called!");
    });
    router.registerNative("app.about", [](const json&) {
        LOG_INFO("[Command] app.about: BerkIDE v", BERKIDE_VERSION);
    });

    // ========================================================================
    // MUTATION COMMANDS — New subsystem operations accessible via Tier 1 API
    // MUTASYON KOMUTLARI — Tier 1 API uzerinden erisilebilir yeni alt sistem islemleri
    // ========================================================================

    // --- process.spawn: Spawn a subprocess ---
    // --- process.spawn: Alt surec baslat ---
    router.registerNative("process.spawn", [ctx](const json& args) {
        if (!ctx || !ctx->processManager) return;
        std::string command = args.value("command", "");
        if (command.empty()) return;
        std::vector<std::string> procArgs;
        if (args.contains("args") && args["args"].is_array()) {
            for (auto& a : args["args"]) procArgs.push_back(a.get<std::string>());
        }
        int id = ctx->processManager->spawn(command, procArgs);
        LOG_INFO("[Command] process.spawn: id=", id, " cmd=", command);
    });

    // --- process.write: Write data to subprocess stdin ---
    // --- process.write: Alt surecin stdin'ine veri yaz ---
    router.registerNative("process.write", [ctx](const json& args) {
        if (!ctx || !ctx->processManager) return;
        int id = args.value("id", -1);
        std::string data = args.value("data", "");
        if (id >= 0) ctx->processManager->write(id, data);
    });

    // --- process.closeStdin: Close subprocess stdin ---
    // --- process.closeStdin: Alt surecin stdin'ini kapat ---
    router.registerNative("process.closeStdin", [ctx](const json& args) {
        if (!ctx || !ctx->processManager) return;
        int id = args.value("id", -1);
        if (id >= 0) ctx->processManager->closeStdin(id);
    });

    // --- process.kill: Kill a subprocess ---
    // --- process.kill: Alt sureci sonlandir ---
    router.registerNative("process.kill", [ctx](const json& args) {
        if (!ctx || !ctx->processManager) return;
        int id = args.value("id", -1);
        if (id >= 0) ctx->processManager->kill(id);
    });

    // --- process.signal: Send signal to subprocess ---
    // --- process.signal: Alt surece sinyal gonder ---
    router.registerNative("process.signal", [ctx](const json& args) {
        if (!ctx || !ctx->processManager) return;
        int id = args.value("id", -1);
        int sig = args.value("signal", 15);
        if (id >= 0) ctx->processManager->signal(id, sig);
    });

    // --- treesitter.loadLanguage: Load a tree-sitter language grammar ---
    // --- treesitter.loadLanguage: Bir tree-sitter dil grameri yukle ---
    router.registerNative("treesitter.loadLanguage", [ctx](const json& args) {
        if (!ctx || !ctx->treeSitter) return;
        std::string name = args.value("name", "");
        std::string path = args.value("path", "");
        if (!name.empty() && !path.empty()) ctx->treeSitter->loadLanguage(name, path);
    });

    // --- treesitter.setLanguage: Set active language for parsing ---
    // --- treesitter.setLanguage: Ayristirma icin aktif dili ayarla ---
    router.registerNative("treesitter.setLanguage", [ctx](const json& args) {
        if (!ctx || !ctx->treeSitter) return;
        std::string name = args.value("name", "");
        if (!name.empty()) ctx->treeSitter->setLanguage(name);
    });

    // --- treesitter.parse: Parse source code with current language ---
    // --- treesitter.parse: Mevcut dille kaynak kodu ayristir ---
    router.registerNative("treesitter.parse", [ctx](const json& args) {
        if (!ctx || !ctx->treeSitter) return;
        std::string source = args.value("source", "");
        ctx->treeSitter->parse(source);
    });

    // --- treesitter.reset: Clear the syntax tree ---
    // --- treesitter.reset: Sozdizimi agacini temizle ---
    router.registerNative("treesitter.reset", [ctx](const json& args) {
        if (!ctx || !ctx->treeSitter) return;
        ctx->treeSitter->reset();
    });

    // --- plugins.enable: Enable a plugin by name ---
    // --- plugins.enable: Adla bir eklentiyi etkinlestir ---
    router.registerNative("plugins.enable", [ctx](const json& args) {
        if (!ctx || !ctx->pluginManager) return;
        std::string name = args.value("name", "");
        if (!name.empty()) ctx->pluginManager->enable(name);
    });

    // --- plugins.disable: Disable a plugin by name ---
    // --- plugins.disable: Adla bir eklentiyi devre disi birak ---
    router.registerNative("plugins.disable", [ctx](const json& args) {
        if (!ctx || !ctx->pluginManager) return;
        std::string name = args.value("name", "");
        if (!name.empty()) ctx->pluginManager->disable(name);
    });

    // --- extmarks.set: Set a text decoration ---
    // --- extmarks.set: Metin dekorasyonu ayarla ---
    router.registerNative("extmarks.set", [ctx](const json& args) {
        if (!ctx || !ctx->extmarkManager) return;
        std::string ns = args.value("namespace", "default");
        int sl = args.value("startLine", 0), sc = args.value("startCol", 0);
        int el = args.value("endLine", sl), ec = args.value("endCol", sc);
        std::string type = args.value("type", "");
        std::string data = args.value("data", "");
        ctx->extmarkManager->set(ns, sl, sc, el, ec, type, data);
    });

    // --- extmarks.remove: Remove an extmark by ID ---
    // --- extmarks.remove: ID ile bir extmark'i kaldir ---
    router.registerNative("extmarks.remove", [ctx](const json& args) {
        if (!ctx || !ctx->extmarkManager) return;
        int id = args.value("id", -1);
        if (id >= 0) ctx->extmarkManager->remove(id);
    });

    // --- extmarks.clearNamespace: Clear all extmarks in a namespace ---
    // --- extmarks.clearNamespace: Bir ad alanindaki tum extmark'leri temizle ---
    router.registerNative("extmarks.clearNamespace", [ctx](const json& args) {
        if (!ctx || !ctx->extmarkManager) return;
        std::string ns = args.value("namespace", "");
        if (!ns.empty()) ctx->extmarkManager->clearNamespace(ns);
    });

    // --- extmarks.clearAll: Clear all extmarks ---
    // --- extmarks.clearAll: Tum extmark'leri temizle ---
    router.registerNative("extmarks.clearAll", [ctx](const json& args) {
        if (!ctx || !ctx->extmarkManager) return;
        ctx->extmarkManager->clearAll();
    });

    // --- selection.setAnchor: Set selection anchor point ---
    // --- selection.setAnchor: Secim baglama noktasini ayarla ---
    router.registerNative("selection.setAnchor", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        int line = args.value("line", -1), col = args.value("col", -1);
        if (line < 0 || col < 0) return;
        ctx->buffers->active().getSelection().setAnchor(line, col);
        if (ctx->eventBus) ctx->eventBus->emit("selectionChanged");
    });

    // --- selection.clear: Clear current selection ---
    // --- selection.clear: Mevcut secimi temizle ---
    router.registerNative("selection.clear", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        ctx->buffers->active().getSelection().clear();
        if (ctx->eventBus) ctx->eventBus->emit("selectionChanged");
    });

    // --- selection.setType: Set selection type (char/line/block) ---
    // --- selection.setType: Secim turunu ayarla (karakter/satir/blok) ---
    router.registerNative("selection.setType", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        std::string t = args.value("type", "char");
        auto& sel = ctx->buffers->active().getSelection();
        if (t == "line") sel.setType(SelectionType::Line);
        else if (t == "block") sel.setType(SelectionType::Block);
        else sel.setType(SelectionType::Char);
    });

    // --- registers.set: Set a named register ---
    // --- registers.set: Adlandirilmis register ayarla ---
    router.registerNative("registers.set", [ctx](const json& args) {
        if (!ctx || !ctx->registers) return;
        std::string name = args.value("name", "");
        std::string content = args.value("content", "");
        bool linewise = args.value("linewise", false);
        if (!name.empty()) ctx->registers->set(name, content, linewise);
    });

    // --- registers.clear: Clear all registers ---
    // --- registers.clear: Tum register'lari temizle ---
    router.registerNative("registers.clear", [ctx](const json& args) {
        if (!ctx || !ctx->registers) return;
        ctx->registers->clearAll();
    });

    // --- marks.remove: Remove a named mark ---
    // --- marks.remove: Adlandirilmis isareti kaldir ---
    router.registerNative("marks.remove", [ctx](const json& args) {
        if (!ctx || !ctx->markManager) return;
        std::string name = args.value("name", "");
        if (!name.empty()) ctx->markManager->remove(name);
    });

    // --- marks.clear: Clear all buffer-local marks ---
    // --- marks.clear: Tum buffer-yerel isaretleri temizle ---
    router.registerNative("marks.clear", [ctx](const json& args) {
        if (!ctx || !ctx->markManager) return;
        ctx->markManager->clearLocal();
    });

    // --- macro.clear: Clear a macro register ---
    // --- macro.clear: Makro register'ini temizle ---
    router.registerNative("macro.clear", [ctx](const json& args) {
        if (!ctx || !ctx->macroRecorder) return;
        std::string reg = args.value("register", "q");
        ctx->macroRecorder->clearRegister(reg);
    });

    // --- keymap.createKeymap: Create a new keymap ---
    // --- keymap.createKeymap: Yeni tus haritasi olustur ---
    router.registerNative("keymap.createKeymap", [ctx](const json& args) {
        if (!ctx || !ctx->keymapManager) return;
        std::string name = args.value("name", "");
        std::string parent = args.value("parent", "");
        if (!name.empty()) ctx->keymapManager->createKeymap(name, parent);
    });

    // --- chars.addWordChar: Add a character to word class ---
    // --- chars.addWordChar: Kelime sinifina karakter ekle ---
    router.registerNative("chars.addWordChar", [ctx](const json& args) {
        if (!ctx || !ctx->charClassifier) return;
        std::string ch = args.value("char", "");
        if (!ch.empty()) ctx->charClassifier->addWordChar(ch[0]);
    });

    // --- chars.addBracketPair: Add a custom bracket pair ---
    // --- chars.addBracketPair: Ozel parantez cifti ekle ---
    router.registerNative("chars.addBracketPair", [ctx](const json& args) {
        if (!ctx || !ctx->charClassifier) return;
        std::string open = args.value("open", "");
        std::string close = args.value("close", "");
        if (!open.empty() && !close.empty())
            ctx->charClassifier->addBracketPair(open[0], close[0]);
    });

    // --- completion.setMaxResults: Set max completion results ---
    // --- completion.setMaxResults: Maks tamamlama sonucu sayisini ayarla ---
    router.registerNative("completion.setMaxResults", [ctx](const json& args) {
        if (!ctx || !ctx->completionEngine) return;
        int max = args.value("max", 50);
        ctx->completionEngine->setMaxResults(max);
    });

    // --- indent.setConfig: Set indent configuration ---
    // --- indent.setConfig: Girinti yapilandirmasini ayarla ---
    router.registerNative("indent.setConfig", [ctx](const json& args) {
        if (!ctx || !ctx->indentEngine) return;
        auto cfg = ctx->indentEngine->config();
        if (args.contains("useTabs")) cfg.useTabs = args["useTabs"].get<bool>();
        if (args.contains("tabWidth")) cfg.tabWidth = args["tabWidth"].get<int>();
        if (args.contains("shiftWidth")) cfg.shiftWidth = args["shiftWidth"].get<int>();
        ctx->indentEngine->setConfig(cfg);
    });

    // --- autosave.start: Start auto-save ---
    // --- autosave.start: Otomatik kaydetmeyi baslat ---
    router.registerNative("autosave.start", [ctx](const json& args) {
        if (!ctx || !ctx->autoSave) return;
        ctx->autoSave->start();
    });

    // --- autosave.stop: Stop auto-save ---
    // --- autosave.stop: Otomatik kaydetmeyi durdur ---
    router.registerNative("autosave.stop", [ctx](const json& args) {
        if (!ctx || !ctx->autoSave) return;
        ctx->autoSave->stop();
    });

    // --- autosave.setInterval: Set auto-save interval in seconds ---
    // --- autosave.setInterval: Otomatik kaydetme araligini saniye cinsinden ayarla ---
    router.registerNative("autosave.setInterval", [ctx](const json& args) {
        if (!ctx || !ctx->autoSave) return;
        int seconds = args.value("seconds", 60);
        ctx->autoSave->setInterval(seconds);
    });

    // --- workers.postMessage: Send message to a worker ---
    // --- workers.postMessage: Calisana mesaj gonder ---
    router.registerNative("workers.postMessage", [ctx](const json& args) {
        if (!ctx || !ctx->workerManager) return;
        int id = args.value("id", -1);
        std::string message = args.value("message", "");
        if (id >= 0) ctx->workerManager->postMessage(id, message);
    });

    // --- window.setActive: Set active window by ID ---
    // --- window.setActive: ID ile aktif pencereyi ayarla ---
    router.registerNative("window.setActive", [ctx](const json& args) {
        if (!ctx || !ctx->windowManager) return;
        int id = args.value("id", -1);
        if (id >= 0) ctx->windowManager->setActive(id);
        if (ctx->eventBus) ctx->eventBus->emit("windowChanged");
    });

    // --- window.focusUp/Down/Left/Right: Directional window focus ---
    // --- window.focusUp/Down/Left/Right: Yonlu pencere odaklama ---
    router.registerNative("window.focusUp", [ctx](const json&) {
        if (!ctx || !ctx->windowManager) return;
        ctx->windowManager->focusUp();
        if (ctx->eventBus) ctx->eventBus->emit("windowChanged");
    });
    router.registerNative("window.focusDown", [ctx](const json&) {
        if (!ctx || !ctx->windowManager) return;
        ctx->windowManager->focusDown();
        if (ctx->eventBus) ctx->eventBus->emit("windowChanged");
    });
    router.registerNative("window.focusLeft", [ctx](const json&) {
        if (!ctx || !ctx->windowManager) return;
        ctx->windowManager->focusLeft();
        if (ctx->eventBus) ctx->eventBus->emit("windowChanged");
    });
    router.registerNative("window.focusRight", [ctx](const json&) {
        if (!ctx || !ctx->windowManager) return;
        ctx->windowManager->focusRight();
        if (ctx->eventBus) ctx->eventBus->emit("windowChanged");
    });

    // ========================================================================
    // QUERY COMMANDS — Read-only operations that return JSON data via Tier 1
    // SORGU KOMUTLARI — Tier 1 uzerinden JSON veri donduren salt okunur islemler
    // ========================================================================

    // --- diff.compute: Compute line diff between two texts ---
    // --- diff.compute: Iki metin arasinda satir farki hesapla ---
    router.registerQuery("diff.compute", [ctx](const json& args) -> json {
        if (!ctx || !ctx->diffEngine) return json::array();
        std::string oldText = args.value("oldText", "");
        std::string newText = args.value("newText", "");
        auto hunks = ctx->diffEngine->diffText(oldText, newText);
        json result = json::array();
        for (auto& h : hunks) {
            result.push_back({{"oldStart", h.oldStart}, {"oldCount", h.oldCount},
                              {"newStart", h.newStart}, {"newCount", h.newCount},
                              {"oldLines", h.oldLines}, {"newLines", h.newLines}});
        }
        return result;
    });

    // --- diff.unified: Generate unified diff string ---
    // --- diff.unified: Birlesik fark dizesi olustur ---
    router.registerQuery("diff.unified", [ctx](const json& args) -> json {
        if (!ctx || !ctx->diffEngine) return "";
        std::string oldText = args.value("oldText", "");
        std::string newText = args.value("newText", "");
        std::string oldName = args.value("oldName", "a");
        std::string newName = args.value("newName", "b");
        auto hunks = ctx->diffEngine->diffText(oldText, newText);
        return ctx->diffEngine->unifiedDiff(hunks, oldName, newName);
    });

    // --- diff.merge3: Three-way merge ---
    // --- diff.merge3: Uc yonlu birlestirme ---
    router.registerQuery("diff.merge3", [ctx](const json& args) -> json {
        if (!ctx || !ctx->diffEngine) return json({{"error", "no diff engine"}});
        auto toLines = [](const std::string& text) {
            std::vector<std::string> lines;
            std::istringstream ss(text);
            std::string line;
            while (std::getline(ss, line)) lines.push_back(line);
            return lines;
        };
        auto base = toLines(args.value("base", ""));
        auto ours = toLines(args.value("ours", ""));
        auto theirs = toLines(args.value("theirs", ""));
        auto result = ctx->diffEngine->merge3(base, ours, theirs);
        return json({{"lines", result.lines}, {"hasConflicts", result.hasConflicts},
                     {"conflictCount", result.conflictCount}});
    });

    // --- completion.filter: Filter completion candidates by query ---
    // --- completion.filter: Tamamlama adaylarini sorguya gore filtrele ---
    router.registerQuery("completion.filter", [ctx](const json& args) -> json {
        if (!ctx || !ctx->completionEngine) return json::array();
        std::string query = args.value("query", "");
        std::vector<CompletionItem> candidates;
        if (args.contains("candidates") && args["candidates"].is_array()) {
            for (auto& c : args["candidates"]) {
                CompletionItem item;
                item.label = c.is_string() ? c.get<std::string>() : c.value("label", "");
                item.detail = c.is_object() ? c.value("detail", "") : "";
                candidates.push_back(item);
            }
        }
        auto results = ctx->completionEngine->filter(candidates, query);
        json arr = json::array();
        for (auto& r : results) {
            arr.push_back({{"label", r.label}, {"detail", r.detail}, {"score", r.score}});
        }
        return arr;
    });

    // --- completion.score: Score a single candidate against query ---
    // --- completion.score: Tek bir adayi sorguya gore puanla ---
    router.registerQuery("completion.score", [ctx](const json& args) -> json {
        if (!ctx || !ctx->completionEngine) return 0.0;
        std::string text = args.value("text", "");
        std::string query = args.value("query", "");
        return ctx->completionEngine->score(text, query);
    });

    // --- completion.extractWords: Extract words from text ---
    // --- completion.extractWords: Metinden kelimeleri cikar ---
    router.registerQuery("completion.extractWords", [ctx](const json& args) -> json {
        if (!ctx || !ctx->completionEngine) return json::array();
        std::string text = args.value("text", "");
        return ctx->completionEngine->extractWords(text);
    });

    // --- chars.classify: Classify a character type ---
    // --- chars.classify: Karakter turunu siniflandir ---
    router.registerQuery("chars.classify", [ctx](const json& args) -> json {
        if (!ctx || !ctx->charClassifier) return "unknown";
        std::string ch = args.value("char", "");
        if (ch.empty()) return "unknown";
        auto type = ctx->charClassifier->classify(ch[0]);
        switch (type) {
            case CharType::Word: return "word";
            case CharType::Whitespace: return "whitespace";
            case CharType::Punctuation: return "punctuation";
            case CharType::LineBreak: return "linebreak";
            default: return "other";
        }
    });

    // --- chars.isWord: Check if character is a word character ---
    // --- chars.isWord: Karakterin kelime karakteri olup olmadigini kontrol et ---
    router.registerQuery("chars.isWord", [ctx](const json& args) -> json {
        if (!ctx || !ctx->charClassifier) return false;
        std::string ch = args.value("char", "");
        return !ch.empty() && ctx->charClassifier->isWord(ch[0]);
    });

    // --- chars.wordAt: Get word at buffer position ---
    // --- chars.wordAt: Buffer konumundaki kelimeyi al ---
    router.registerQuery("chars.wordAt", [ctx](const json& args) -> json {
        if (!ctx || !ctx->charClassifier || !ctx->buffers) return nullptr;
        int line = args.value("line", -1), col = args.value("col", -1);
        if (line < 0 || col < 0) return nullptr;
        auto& buf = ctx->buffers->active().getBuffer();
        if (line >= buf.lineCount()) return nullptr;
        auto wr = ctx->charClassifier->wordAt(buf.getLine(line), col);
        return json({{"startCol", wr.startCol}, {"endCol", wr.endCol}, {"text", wr.text}});
    });

    // --- chars.matchBracket: Find matching bracket ---
    // --- chars.matchBracket: Eslesen parantezi bul ---
    router.registerQuery("chars.matchBracket", [ctx](const json& args) -> json {
        if (!ctx || !ctx->charClassifier || !ctx->buffers) return nullptr;
        int line = args.value("line", -1), col = args.value("col", -1);
        if (line < 0 || col < 0) return nullptr;
        auto match = ctx->charClassifier->findMatchingBracket(ctx->buffers->active().getBuffer(), line, col);
        if (!match.found) return nullptr;
        return json({{"line", match.line}, {"col", match.col}, {"bracket", std::string(1, match.bracket)}});
    });

    // --- chars.nextWordStart: Find next word start position ---
    // --- chars.nextWordStart: Sonraki kelime baslangic konumunu bul ---
    router.registerQuery("chars.nextWordStart", [ctx](const json& args) -> json {
        if (!ctx || !ctx->charClassifier || !ctx->buffers) return -1;
        int line = args.value("line", -1), col = args.value("col", -1);
        if (line < 0 || col < 0) return -1;
        auto& buf = ctx->buffers->active().getBuffer();
        if (line >= buf.lineCount()) return -1;
        return ctx->charClassifier->nextWordStart(buf.getLine(line), col);
    });

    // --- chars.prevWordStart: Find previous word start position ---
    // --- chars.prevWordStart: Onceki kelime baslangic konumunu bul ---
    router.registerQuery("chars.prevWordStart", [ctx](const json& args) -> json {
        if (!ctx || !ctx->charClassifier || !ctx->buffers) return -1;
        int line = args.value("line", -1), col = args.value("col", -1);
        if (line < 0 || col < 0) return -1;
        auto& buf = ctx->buffers->active().getBuffer();
        if (line >= buf.lineCount()) return -1;
        return ctx->charClassifier->prevWordStart(buf.getLine(line), col);
    });

    // --- chars.wordEnd: Find end of word at position ---
    // --- chars.wordEnd: Konumdaki kelimenin sonunu bul ---
    router.registerQuery("chars.wordEnd", [ctx](const json& args) -> json {
        if (!ctx || !ctx->charClassifier || !ctx->buffers) return -1;
        int line = args.value("line", -1), col = args.value("col", -1);
        if (line < 0 || col < 0) return -1;
        auto& buf = ctx->buffers->active().getBuffer();
        if (line >= buf.lineCount()) return -1;
        return ctx->charClassifier->wordEnd(buf.getLine(line), col);
    });

    // --- encoding.detectFile: Detect file encoding ---
    // --- encoding.detectFile: Dosya kodlamasini tespit et ---
    router.registerQuery("encoding.detectFile", [ctx](const json& args) -> json {
        if (!ctx || !ctx->encodingDetector) return nullptr;
        std::string path = args.value("path", "");
        if (path.empty()) return nullptr;
        auto result = ctx->encodingDetector->detectFile(path);
        return json({{"encoding", ctx->encodingDetector->encodingName(result.encoding)},
                     {"hasBOM", result.hasBOM}, {"confidence", result.confidence}});
    });

    // --- encoding.isValidUTF8: Check if text is valid UTF-8 ---
    // --- encoding.isValidUTF8: Metnin gecerli UTF-8 olup olmadigini kontrol et ---
    router.registerQuery("encoding.isValidUTF8", [ctx](const json& args) -> json {
        if (!ctx || !ctx->encodingDetector) return false;
        std::string text = args.value("text", "");
        return ctx->encodingDetector->isValidUTF8(
            reinterpret_cast<const uint8_t*>(text.data()), text.size());
    });

    // --- encoding.name: Get encoding name string ---
    // --- encoding.name: Kodlama adi dizesini al ---
    router.registerQuery("encoding.name", [ctx](const json& args) -> json {
        if (!ctx || !ctx->encodingDetector) return "";
        std::string enc = args.value("encoding", "");
        auto parsed = ctx->encodingDetector->parseEncoding(enc);
        return ctx->encodingDetector->encodingName(parsed);
    });

    // --- process.isRunning: Check if a subprocess is running ---
    // --- process.isRunning: Bir alt surecin calisip calismadigini kontrol et ---
    router.registerQuery("process.isRunning", [ctx](const json& args) -> json {
        if (!ctx || !ctx->processManager) return false;
        int id = args.value("id", -1);
        return id >= 0 && ctx->processManager->isRunning(id);
    });

    // --- process.list: List all managed processes ---
    // --- process.list: Tum yonetilen surecleri listele ---
    router.registerQuery("process.list", [ctx](const json& args) -> json {
        if (!ctx || !ctx->processManager) return json::array();
        auto procs = ctx->processManager->list();
        json arr = json::array();
        for (auto& p : procs) {
            arr.push_back({{"id", p.id}, {"pid", p.pid},
                           {"running", p.running}, {"exitCode", p.exitCode}});
        }
        return arr;
    });

    // --- treesitter.currentLanguage: Get current parsing language ---
    // --- treesitter.currentLanguage: Mevcut ayristirma dilini al ---
    router.registerQuery("treesitter.currentLanguage", [ctx](const json& args) -> json {
        if (!ctx || !ctx->treeSitter) return "";
        return ctx->treeSitter->currentLanguage();
    });

    // --- treesitter.hasLanguage: Check if language is loaded ---
    // --- treesitter.hasLanguage: Dilin yuklu olup olmadigini kontrol et ---
    router.registerQuery("treesitter.hasLanguage", [ctx](const json& args) -> json {
        if (!ctx || !ctx->treeSitter) return false;
        std::string name = args.value("name", "");
        return !name.empty() && ctx->treeSitter->hasLanguage(name);
    });

    // --- treesitter.listLanguages: List loaded languages ---
    // --- treesitter.listLanguages: Yuklu dilleri listele ---
    router.registerQuery("treesitter.listLanguages", [ctx](const json& args) -> json {
        if (!ctx || !ctx->treeSitter) return json::array();
        return ctx->treeSitter->listLanguages();
    });

    // --- treesitter.nodeAt: Get syntax node at position ---
    // --- treesitter.nodeAt: Konumdaki sozdizimi dugumunu al ---
    router.registerQuery("treesitter.nodeAt", [ctx](const json& args) -> json {
        if (!ctx || !ctx->treeSitter || !ctx->treeSitter->hasTree()) return nullptr;
        int line = args.value("line", 0), col = args.value("col", 0);
        auto node = ctx->treeSitter->nodeAt(line, col);
        return json({{"type", node.type}, {"startLine", node.startLine}, {"startCol", node.startCol},
                     {"endLine", node.endLine}, {"endCol", node.endCol}, {"isNamed", node.isNamed}});
    });

    // --- plugins.list: List all plugins with status ---
    // --- plugins.list: Tum eklentileri durumlariyla listele ---
    router.registerQuery("plugins.list", [ctx](const json& args) -> json {
        if (!ctx || !ctx->pluginManager) return json::array();
        auto& plugins = ctx->pluginManager->list();
        json arr = json::array();
        for (auto& p : plugins) {
            arr.push_back({{"name", p.manifest.name}, {"version", p.manifest.version},
                           {"enabled", p.manifest.enabled}, {"loaded", p.loaded}});
        }
        return arr;
    });

    // --- extmarks.get: Get an extmark by ID ---
    // --- extmarks.get: ID ile bir extmark al ---
    router.registerQuery("extmarks.get", [ctx](const json& args) -> json {
        if (!ctx || !ctx->extmarkManager) return nullptr;
        int id = args.value("id", -1);
        if (id < 0) return nullptr;
        auto* em = ctx->extmarkManager->get(id);
        if (!em) return nullptr;
        return json({{"id", em->id}, {"namespace", em->ns}, {"startLine", em->startLine},
                     {"startCol", em->startCol}, {"endLine", em->endLine}, {"endCol", em->endCol},
                     {"type", em->type}, {"data", em->data}});
    });

    // --- extmarks.getInRange: Get extmarks in a line range ---
    // --- extmarks.getInRange: Satir araligindaki extmark'leri al ---
    router.registerQuery("extmarks.getInRange", [ctx](const json& args) -> json {
        if (!ctx || !ctx->extmarkManager) return json::array();
        int sl = args.value("startLine", 0), el = args.value("endLine", sl);
        std::string ns = args.value("namespace", "");
        auto marks = ctx->extmarkManager->getInRange(sl, el, ns);
        json arr = json::array();
        for (auto* em : marks) {
            arr.push_back({{"id", em->id}, {"namespace", em->ns}, {"startLine", em->startLine},
                           {"startCol", em->startCol}, {"endLine", em->endLine}, {"endCol", em->endCol},
                           {"type", em->type}, {"data", em->data}});
        }
        return arr;
    });

    // --- extmarks.list: List all extmarks ---
    // --- extmarks.list: Tum extmark'leri listele ---
    router.registerQuery("extmarks.list", [ctx](const json& args) -> json {
        if (!ctx || !ctx->extmarkManager) return json::array();
        std::string ns = args.value("namespace", "");
        auto marks = ctx->extmarkManager->list(ns);
        json arr = json::array();
        for (auto* em : marks) {
            arr.push_back({{"id", em->id}, {"namespace", em->ns}, {"startLine", em->startLine},
                           {"startCol", em->startCol}, {"endLine", em->endLine}, {"endCol", em->endCol},
                           {"type", em->type}});
        }
        return arr;
    });

    // --- extmarks.count: Count all extmarks ---
    // --- extmarks.count: Tum extmark'leri say ---
    router.registerQuery("extmarks.count", [ctx](const json& args) -> json {
        if (!ctx || !ctx->extmarkManager) return 0;
        return (int)ctx->extmarkManager->count();
    });

    // --- selection.isActive: Check if selection is active ---
    // --- selection.isActive: Secimin aktif olup olmadigini kontrol et ---
    router.registerQuery("selection.isActive", [ctx](const json& args) -> json {
        if (!ctx || !ctx->buffers) return false;
        return ctx->buffers->active().getSelection().isActive();
    });

    // --- selection.getRange: Get selection range ---
    // --- selection.getRange: Secim araligini al ---
    router.registerQuery("selection.getRange", [ctx](const json& args) -> json {
        if (!ctx || !ctx->buffers) return nullptr;
        auto& st = ctx->buffers->active();
        auto& sel = st.getSelection();
        if (!sel.isActive()) return nullptr;
        auto& cur = st.getCursor();
        int sl, sc, el, ec;
        sel.getRange(cur.getLine(), cur.getCol(), sl, sc, el, ec);
        return json({{"startLine", sl}, {"startCol", sc}, {"endLine", el}, {"endCol", ec}});
    });

    // --- selection.getText: Get selected text ---
    // --- selection.getText: Secili metni al ---
    router.registerQuery("selection.getText", [ctx](const json& args) -> json {
        if (!ctx || !ctx->buffers) return "";
        auto& st = ctx->buffers->active();
        auto& sel = st.getSelection();
        if (!sel.isActive()) return "";
        auto& cur = st.getCursor();
        return sel.getText(st.getBuffer(), cur.getLine(), cur.getCol());
    });

    // --- selection.getType: Get selection type ---
    // --- selection.getType: Secim turunu al ---
    router.registerQuery("selection.getType", [ctx](const json& args) -> json {
        if (!ctx || !ctx->buffers) return "none";
        auto t = ctx->buffers->active().getSelection().type();
        switch (t) {
            case SelectionType::Line: return "line";
            case SelectionType::Block: return "block";
            default: return "char";
        }
    });

    // --- registers.get: Get a named register value ---
    // --- registers.get: Adlandirilmis register degerini al ---
    router.registerQuery("registers.get", [ctx](const json& args) -> json {
        if (!ctx || !ctx->registers) return nullptr;
        std::string name = args.value("name", "\"");
        auto entry = ctx->registers->get(name);
        if (entry.content.empty()) return nullptr;
        return json({{"content", entry.content}, {"linewise", entry.linewise}});
    });

    // --- registers.list: List all registers ---
    // --- registers.list: Tum register'lari listele ---
    router.registerQuery("registers.list", [ctx](const json& args) -> json {
        if (!ctx || !ctx->registers) return json::array();
        auto regs = ctx->registers->list();
        json arr = json::array();
        for (auto& [name, entry] : regs) {
            arr.push_back({{"name", name}, {"content", entry.content}, {"linewise", entry.linewise}});
        }
        return arr;
    });

    // --- search.findAll: Find all occurrences of pattern ---
    // --- search.findAll: Kalibin tum oluslarini bul ---
    router.registerQuery("search.findAll", [ctx](const json& args) -> json {
        if (!ctx || !ctx->buffers || !ctx->searchEngine) return json::array();
        std::string pattern = args.value("pattern", "");
        if (pattern.empty()) return json::array();
        SearchOptions opts;
        opts.caseSensitive = args.value("caseSensitive", true);
        opts.regex = args.value("regex", false);
        opts.wholeWord = args.value("wholeWord", false);
        auto matches = ctx->searchEngine->findAll(ctx->buffers->active().getBuffer(), pattern, opts);
        json arr = json::array();
        for (auto& m : matches) {
            arr.push_back({{"line", m.line}, {"col", m.col}, {"length", m.length}});
        }
        return arr;
    });

    // --- windows.list: List all windows ---
    // --- windows.list: Tum pencereleri listele ---
    router.registerQuery("windows.list", [ctx](const json& args) -> json {
        if (!ctx || !ctx->windowManager) return json::array();
        auto ids = ctx->windowManager->listWindowIds();
        json arr = json::array();
        for (int id : ids) {
            auto* win = ctx->windowManager->getWindow(id);
            if (win) arr.push_back({{"id", id}, {"bufferIndex", win->bufferIndex}});
        }
        return arr;
    });

    // --- windows.activeId: Get active window ID ---
    // --- windows.activeId: Aktif pencere ID'sini al ---
    router.registerQuery("windows.activeId", [ctx](const json& args) -> json {
        if (!ctx || !ctx->windowManager) return -1;
        return ctx->windowManager->activeId();
    });

    // --- windows.count: Get window count ---
    // --- windows.count: Pencere sayisini al ---
    router.registerQuery("windows.count", [ctx](const json& args) -> json {
        if (!ctx || !ctx->windowManager) return 0;
        return ctx->windowManager->windowCount();
    });

    // --- marks.get: Get a named mark ---
    // --- marks.get: Adlandirilmis isareti al ---
    router.registerQuery("marks.get", [ctx](const json& args) -> json {
        if (!ctx || !ctx->markManager) return nullptr;
        std::string name = args.value("name", "");
        if (name.empty()) return nullptr;
        auto* m = ctx->markManager->get(name);
        if (!m) return nullptr;
        return json({{"line", m->line}, {"col", m->col}});
    });

    // --- marks.list: List all marks ---
    // --- marks.list: Tum isaretleri listele ---
    router.registerQuery("marks.list", [ctx](const json& args) -> json {
        if (!ctx || !ctx->markManager) return json::array();
        auto marks = ctx->markManager->list();
        json arr = json::array();
        for (auto& [name, m] : marks) {
            arr.push_back({{"name", name}, {"line", m.line}, {"col", m.col}});
        }
        return arr;
    });

    // --- folds.list: List all fold regions ---
    // --- folds.list: Tum katlama bolgelerini listele ---
    router.registerQuery("folds.list", [ctx](const json& args) -> json {
        if (!ctx || !ctx->foldManager) return json::array();
        auto folds = ctx->foldManager->list();
        json arr = json::array();
        for (auto& f : folds) {
            arr.push_back({{"startLine", f.startLine}, {"endLine", f.endLine},
                           {"collapsed", f.collapsed}, {"label", f.label}});
        }
        return arr;
    });

    // --- folds.at: Get fold at a line ---
    // --- folds.at: Satirdaki katlamayi al ---
    router.registerQuery("folds.at", [ctx](const json& args) -> json {
        if (!ctx || !ctx->foldManager) return nullptr;
        int line = args.value("line", -1);
        if (line < 0) return nullptr;
        auto* f = ctx->foldManager->getFoldAt(line);
        if (!f) return nullptr;
        return json({{"startLine", f->startLine}, {"endLine", f->endLine},
                     {"collapsed", f->collapsed}, {"label", f->label}});
    });

    // --- macro.getMacro: Get recorded macro commands ---
    // --- macro.getMacro: Kaydedilmis makro komutlarini al ---
    router.registerQuery("macro.getMacro", [ctx](const json& args) -> json {
        if (!ctx || !ctx->macroRecorder) return nullptr;
        std::string reg = args.value("register", "q");
        auto* macro = ctx->macroRecorder->getMacro(reg);
        if (!macro) return nullptr;
        json arr = json::array();
        for (auto& cmd : *macro) {
            arr.push_back({{"name", cmd.name}, {"args", cmd.argsJson}});
        }
        return arr;
    });

    // --- macro.list: List all macro registers ---
    // --- macro.list: Tum makro register'larini listele ---
    router.registerQuery("macro.list", [ctx](const json& args) -> json {
        if (!ctx || !ctx->macroRecorder) return json::array();
        return ctx->macroRecorder->listRegisters();
    });

    // --- keymap.getBinding: Get a key binding ---
    // --- keymap.getBinding: Bir tus baglamasi al ---
    router.registerQuery("keymap.getBinding", [ctx](const json& args) -> json {
        if (!ctx || !ctx->keymapManager) return nullptr;
        std::string keymapName = args.value("keymap", "global");
        std::string keys = args.value("keys", "");
        if (keys.empty()) return nullptr;
        auto* b = ctx->keymapManager->lookup(keymapName, keys);
        if (!b) return nullptr;
        return json({{"keys", b->keys}, {"command", b->command}, {"argsJson", b->argsJson}});
    });

    // --- keymap.list: List all bindings in a keymap ---
    // --- keymap.list: Bir tus haritasindaki tum baglamalari listele ---
    router.registerQuery("keymap.list", [ctx](const json& args) -> json {
        if (!ctx || !ctx->keymapManager) return json::array();
        std::string keymapName = args.value("keymap", "global");
        auto bindings = ctx->keymapManager->listBindings(keymapName);
        json arr = json::array();
        for (auto& b : bindings) {
            arr.push_back({{"keys", b.keys}, {"command", b.command}, {"argsJson", b.argsJson}});
        }
        return arr;
    });

    // --- keymap.listKeymaps: List all keymap names ---
    // --- keymap.listKeymaps: Tum tus haritasi adlarini listele ---
    router.registerQuery("keymap.listKeymaps", [ctx](const json& args) -> json {
        if (!ctx || !ctx->keymapManager) return json::array();
        return ctx->keymapManager->listKeymaps();
    });

    // --- multicursor.list: List all cursors ---
    // --- multicursor.list: Tum imlecleri listele ---
    router.registerQuery("multicursor.list", [ctx](const json& args) -> json {
        if (!ctx || !ctx->multiCursor) return json::array();
        auto& cursors = ctx->multiCursor->cursors();
        json arr = json::array();
        for (auto& c : cursors) {
            arr.push_back({{"line", c.line}, {"col", c.col}});
        }
        return arr;
    });

    // --- multicursor.primary: Get primary cursor position ---
    // --- multicursor.primary: Birincil imlec konumunu al ---
    router.registerQuery("multicursor.primary", [ctx](const json& args) -> json {
        if (!ctx || !ctx->multiCursor) return nullptr;
        auto& p = ctx->multiCursor->primary();
        return json({{"line", p.line}, {"col", p.col}});
    });

    // --- multicursor.count: Get number of cursors ---
    // --- multicursor.count: Imlec sayisini al ---
    router.registerQuery("multicursor.count", [ctx](const json& args) -> json {
        if (!ctx || !ctx->multiCursor) return 0;
        return ctx->multiCursor->count();
    });

    // --- indent.getConfig: Get current indent configuration ---
    // --- indent.getConfig: Mevcut girinti yapilandirmasini al ---
    router.registerQuery("indent.getConfig", [ctx](const json& args) -> json {
        if (!ctx || !ctx->indentEngine) return nullptr;
        auto cfg = ctx->indentEngine->config();
        return json({{"useTabs", cfg.useTabs}, {"tabWidth", cfg.tabWidth},
                     {"shiftWidth", cfg.shiftWidth}});
    });

    // --- session.listSessions: List saved sessions ---
    // --- session.listSessions: Kayitli oturumlari listele ---
    router.registerQuery("session.listSessions", [ctx](const json& args) -> json {
        if (!ctx || !ctx->sessionManager) return json::array();
        return ctx->sessionManager->listSessions();
    });

    // --- workers.state: Get worker state ---
    // --- workers.state: Calisan durumunu al ---
    router.registerQuery("workers.state", [ctx](const json& args) -> json {
        if (!ctx || !ctx->workerManager) return "unknown";
        int id = args.value("id", -1);
        if (id < 0) return "unknown";
        auto state = ctx->workerManager->getState(id);
        switch (state) {
            case WorkerState::Pending: return "pending";
            case WorkerState::Running: return "running";
            case WorkerState::Stopped: return "stopped";
            case WorkerState::Error: return "error";
            default: return "unknown";
        }
    });

    // --- workers.activeCount: Get number of active workers ---
    // --- workers.activeCount: Aktif calisan sayisini al ---
    router.registerQuery("workers.activeCount", [ctx](const json& args) -> json {
        if (!ctx || !ctx->workerManager) return 0;
        return ctx->workerManager->activeCount();
    });

    // --- autosave.listRecovery: List auto-save recovery files ---
    // --- autosave.listRecovery: Otomatik kaydetme kurtarma dosyalarini listele ---
    router.registerQuery("autosave.listRecovery", [ctx](const json& args) -> json {
        if (!ctx || !ctx->autoSave) return json::array();
        auto files = ctx->autoSave->listRecoveryFiles();
        json arr = json::array();
        for (auto& f : files) {
            arr.push_back({{"originalPath", f.originalPath}, {"recoveryPath", f.recoveryPath}, {"timestamp", f.timestamp}});
        }
        return arr;
    });

    // ========================================================================
    // ADDITIONAL MUTATION COMMANDS — Filling Tier 1 API gaps
    // EK MUTASYON KOMUTLARI — Tier 1 API boslukalarini doldurma
    // ========================================================================

    // --- buffer.deleteRange: Delete a range of text ---
    // --- buffer.deleteRange: Bir metin araligini sil ---
    router.registerNative("buffer.deleteRange", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        int sl = args.value("startLine", -1), sc = args.value("startCol", -1);
        int el = args.value("endLine", -1), ec = args.value("endCol", -1);
        if (sl < 0 || sc < 0 || el < 0 || ec < 0) return;
        auto& st = ctx->buffers->active();
        st.getBuffer().deleteRange(sl, sc, el, ec);
        st.markModified(true);
        if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", st.getFilePath());
    });

    // --- buffer.joinLines: Join two consecutive lines ---
    // --- buffer.joinLines: Ardisik iki satiri birlestir ---
    router.registerNative("buffer.joinLines", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        auto& st = ctx->buffers->active();
        int line = args.value("line", st.getCursor().getLine());
        auto& buf = st.getBuffer();
        if (line >= 0 && line + 1 < buf.lineCount()) {
            buf.joinLines(line, line + 1);
            st.markModified(true);
            if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", st.getFilePath());
        }
    });

    // --- buffer.insertLine: Insert a line at the end or at index ---
    // --- buffer.insertLine: Sona veya indekse satir ekle ---
    router.registerNative("buffer.insertLine", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        auto& st = ctx->buffers->active();
        std::string text = args.value("text", "");
        int at = args.value("at", -1);
        if (at >= 0)
            st.getBuffer().insertLineAt(at, text);
        else
            st.getBuffer().insertLine(text);
        st.markModified(true);
        if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", st.getFilePath());
    });

    // --- buffer.clear: Clear buffer content ---
    // --- buffer.clear: Buffer icerigini temizle ---
    router.registerNative("buffer.clear", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        auto& st = ctx->buffers->active();
        st.getBuffer().clear();
        st.getCursor().setPosition(0, 0);
        st.markModified(true);
        if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", st.getFilePath());
    });

    // --- file.saveAll: Save all open buffers ---
    // --- file.saveAll: Tum acik bufferlari kaydet ---
    router.registerNative("file.saveAll", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        int saved = ctx->buffers->saveAll();
        LOG_INFO("[Command] file.saveAll: saved ", saved, " buffers");
        if (ctx->eventBus) ctx->eventBus->emit("fileSaved", "all");
    });

    // --- tab.closeAt: Close buffer at index ---
    // --- tab.closeAt: Indeksteki bufferi kapat ---
    router.registerNative("tab.closeAt", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        int index = args.value("index", -1);
        if (index >= 0) ctx->buffers->closeAt(static_cast<size_t>(index));
        if (ctx->eventBus) ctx->eventBus->emit("tabChanged");
    });

    // --- edit.beginGroup: Begin undo group ---
    // --- edit.beginGroup: Geri alma grubu baslat ---
    router.registerNative("edit.beginGroup", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        ctx->buffers->active().getUndo().beginGroup();
    });

    // --- edit.endGroup: End undo group ---
    // --- edit.endGroup: Geri alma grubunu bitir ---
    router.registerNative("edit.endGroup", [ctx](const json&) {
        if (!ctx || !ctx->buffers) return;
        ctx->buffers->active().getUndo().endGroup();
    });

    // --- undo.branch: Switch to a different undo branch ---
    // --- undo.branch: Farkli bir geri alma dalina gec ---
    router.registerNative("undo.branch", [ctx](const json& args) {
        if (!ctx || !ctx->buffers) return;
        int index = args.value("index", 0);
        ctx->buffers->active().getUndo().branch(index);
    });

    // --- fold.remove: Remove a fold region ---
    // --- fold.remove: Bir katlama bolgesini kaldir ---
    router.registerNative("fold.remove", [ctx](const json& args) {
        if (!ctx || !ctx->foldManager) return;
        int startLine = args.value("startLine", -1);
        if (startLine >= 0) ctx->foldManager->remove(startLine);
        if (ctx->eventBus) ctx->eventBus->emit("foldChanged");
    });

    // --- fold.clearAll: Clear all folds ---
    // --- fold.clearAll: Tum katlamalari temizle ---
    router.registerNative("fold.clearAll", [ctx](const json&) {
        if (!ctx || !ctx->foldManager) return;
        ctx->foldManager->clearAll();
        if (ctx->eventBus) ctx->eventBus->emit("foldChanged");
    });

    // --- marks.clearAll: Clear all marks (local + global) ---
    // --- marks.clearAll: Tum isaretleri temizle (yerel + global) ---
    router.registerNative("marks.clearAll", [ctx](const json&) {
        if (!ctx || !ctx->markManager) return;
        ctx->markManager->clearAll();
    });

    // --- marks.prevChange: Navigate to previous change ---
    // --- marks.prevChange: Onceki degisiklige git ---
    router.registerNative("marks.prevChange", [ctx](const json&) {
        if (!ctx || !ctx->markManager || !ctx->buffers) return;
        JumpEntry entry;
        if (ctx->markManager->prevChange(entry)) {
            ctx->buffers->active().getCursor().setPosition(entry.line, entry.col);
            if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
        }
    });

    // --- marks.nextChange: Navigate to next change ---
    // --- marks.nextChange: Sonraki degisiklige git ---
    router.registerNative("marks.nextChange", [ctx](const json&) {
        if (!ctx || !ctx->markManager || !ctx->buffers) return;
        JumpEntry entry;
        if (ctx->markManager->nextChange(entry)) {
            ctx->buffers->active().getCursor().setPosition(entry.line, entry.col);
            if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
        }
    });

    // --- macro.clearAll: Clear all macro registers ---
    // --- macro.clearAll: Tum makro registerlarini temizle ---
    router.registerNative("macro.clearAll", [ctx](const json&) {
        if (!ctx || !ctx->macroRecorder) return;
        for (auto& reg : ctx->macroRecorder->listRegisters()) {
            ctx->macroRecorder->clearRegister(reg);
        }
    });

    // --- keymap.feedKey: Feed a key into the prefix state machine ---
    // --- keymap.feedKey: Onek durum makinesine bir tus besle ---
    router.registerNative("keymap.feedKey", [ctx](const json& args) {
        if (!ctx || !ctx->keymapManager) return;
        std::string keymap = args.value("keymap", "global");
        std::string key = args.value("key", "");
        if (!key.empty()) ctx->keymapManager->feedKey(keymap, key);
    });

    // --- keymap.resetPrefix: Reset prefix state ---
    // --- keymap.resetPrefix: Onek durumunu sifirla ---
    router.registerNative("keymap.resetPrefix", [ctx](const json&) {
        if (!ctx || !ctx->keymapManager) return;
        ctx->keymapManager->resetPrefix();
    });

    // --- multicursor.add: Add cursor at specific position ---
    // --- multicursor.add: Belirli bir konuma imlec ekle ---
    router.registerNative("multicursor.add", [ctx](const json& args) {
        if (!ctx || !ctx->multiCursor) return;
        int line = args.value("line", -1), col = args.value("col", -1);
        if (line >= 0 && col >= 0) ctx->multiCursor->addCursor(line, col);
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });

    // --- multicursor.remove: Remove cursor by index ---
    // --- multicursor.remove: Dizine gore imleci kaldir ---
    router.registerNative("multicursor.remove", [ctx](const json& args) {
        if (!ctx || !ctx->multiCursor) return;
        int index = args.value("index", -1);
        if (index >= 0) ctx->multiCursor->removeCursor(index);
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });

    // --- multicursor.setPrimary: Set primary cursor position ---
    // --- multicursor.setPrimary: Birincil imlec konumunu ayarla ---
    router.registerNative("multicursor.setPrimary", [ctx](const json& args) {
        if (!ctx || !ctx->multiCursor) return;
        int line = args.value("line", -1), col = args.value("col", -1);
        if (line >= 0 && col >= 0) ctx->multiCursor->setPrimary(line, col);
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });

    // --- multicursor.moveAllUp/Down/Left/Right: Move all cursors ---
    // --- multicursor.tumunuTasi: Tum imlecleri tasi ---
    router.registerNative("multicursor.moveAllUp", [ctx](const json&) {
        if (!ctx || !ctx->multiCursor || !ctx->buffers) return;
        ctx->multiCursor->moveAllUp(ctx->buffers->active().getBuffer());
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });
    router.registerNative("multicursor.moveAllDown", [ctx](const json&) {
        if (!ctx || !ctx->multiCursor || !ctx->buffers) return;
        ctx->multiCursor->moveAllDown(ctx->buffers->active().getBuffer());
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });
    router.registerNative("multicursor.moveAllLeft", [ctx](const json&) {
        if (!ctx || !ctx->multiCursor || !ctx->buffers) return;
        ctx->multiCursor->moveAllLeft(ctx->buffers->active().getBuffer());
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });
    router.registerNative("multicursor.moveAllRight", [ctx](const json&) {
        if (!ctx || !ctx->multiCursor || !ctx->buffers) return;
        ctx->multiCursor->moveAllRight(ctx->buffers->active().getBuffer());
        if (ctx->eventBus) ctx->eventBus->emit("cursorMoved");
    });

    // --- multicursor.insertAtAll: Insert text at all cursors ---
    // --- multicursor.tumundeEkle: Tum imleclere metin ekle ---
    router.registerNative("multicursor.insertAtAll", [ctx](const json& args) {
        if (!ctx || !ctx->multiCursor || !ctx->buffers) return;
        std::string text = args.value("text", "");
        if (text.empty()) return;
        ctx->multiCursor->insertAtAll(ctx->buffers->active().getBuffer(), text);
        ctx->buffers->active().markModified(true);
        if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", ctx->buffers->active().getFilePath());
    });

    // --- multicursor.backspaceAtAll: Backspace at all cursors ---
    // --- multicursor.tumundeSil: Tum imleclerde geri sil ---
    router.registerNative("multicursor.backspaceAtAll", [ctx](const json&) {
        if (!ctx || !ctx->multiCursor || !ctx->buffers) return;
        ctx->multiCursor->backspaceAtAll(ctx->buffers->active().getBuffer());
        ctx->buffers->active().markModified(true);
        if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", ctx->buffers->active().getFilePath());
    });

    // --- multicursor.deleteAtAll: Delete at all cursors ---
    // --- multicursor.tumundeSilIleri: Tum imleclerde ileri sil ---
    router.registerNative("multicursor.deleteAtAll", [ctx](const json&) {
        if (!ctx || !ctx->multiCursor || !ctx->buffers) return;
        ctx->multiCursor->deleteAtAll(ctx->buffers->active().getBuffer());
        ctx->buffers->active().markModified(true);
        if (ctx->eventBus) ctx->eventBus->emit("bufferChanged", ctx->buffers->active().getFilePath());
    });

    // --- window.closeById: Close window by ID ---
    // --- window.closeById: ID ile pencereyi kapat ---
    router.registerNative("window.closeById", [ctx](const json& args) {
        if (!ctx || !ctx->windowManager) return;
        int id = args.value("id", -1);
        if (id >= 0) ctx->windowManager->closeWindow(id);
        if (ctx->eventBus) ctx->eventBus->emit("windowChanged");
    });

    // --- window.resize: Resize active split ratio ---
    // --- window.resize: Aktif bolme oranini yeniden boyutlandir ---
    router.registerNative("window.resize", [ctx](const json& args) {
        if (!ctx || !ctx->windowManager) return;
        double delta = args.value("delta", 0.0);
        ctx->windowManager->resizeActive(delta);
        if (ctx->eventBus) ctx->eventBus->emit("windowChanged");
    });

    // --- window.setLayout: Set total layout dimensions ---
    // --- window.setLayout: Toplam duzen boyutlarini ayarla ---
    router.registerNative("window.setLayout", [ctx](const json& args) {
        if (!ctx || !ctx->windowManager) return;
        int w = args.value("width", 80), h = args.value("height", 24);
        ctx->windowManager->setLayoutSize(w, h);
        ctx->windowManager->recalcLayout();
    });

    // --- file.rename: Rename a file ---
    // --- file.rename: Dosyayi yeniden adlandir ---
    router.registerNative("file.rename", [ctx](const json& args) {
        std::string from = args.value("from", "");
        std::string to = args.value("to", "");
        if (!from.empty() && !to.empty()) FileSystem::renameFile(from, to);
    });

    // --- file.delete: Delete a file ---
    // --- file.delete: Dosyayi sil ---
    router.registerNative("file.delete", [ctx](const json& args) {
        std::string path = args.value("path", "");
        if (!path.empty()) FileSystem::deleteFile(path);
    });

    // --- file.copy: Copy a file ---
    // --- file.copy: Dosyayi kopyala ---
    router.registerNative("file.copy", [ctx](const json& args) {
        std::string src = args.value("src", "");
        std::string dst = args.value("dst", "");
        if (!src.empty() && !dst.empty()) FileSystem::copyFile(src, dst);
    });

    // --- autosave.setDirectory: Set auto-save directory ---
    // --- autosave.setDirectory: Otomatik kaydetme dizinini ayarla ---
    router.registerNative("autosave.setDirectory", [ctx](const json& args) {
        if (!ctx || !ctx->autoSave) return;
        std::string dir = args.value("dir", "");
        if (!dir.empty()) ctx->autoSave->setDirectory(dir);
    });

    // --- autosave.createBackup: Create backup of a file ---
    // --- autosave.createBackup: Dosyanin yedegini olustur ---
    router.registerNative("autosave.createBackup", [ctx](const json& args) {
        if (!ctx || !ctx->autoSave) return;
        std::string path = args.value("path", "");
        if (!path.empty()) ctx->autoSave->createBackup(path);
    });

    // --- autosave.removeRecovery: Remove a recovery file ---
    // --- autosave.removeRecovery: Kurtarma dosyasini kaldir ---
    router.registerNative("autosave.removeRecovery", [ctx](const json& args) {
        if (!ctx || !ctx->autoSave) return;
        std::string path = args.value("path", "");
        if (!path.empty()) ctx->autoSave->removeRecovery(path);
    });

    // --- plugins.discover: Discover plugins in directory ---
    // --- plugins.discover: Dizindeki eklentileri kesfet ---
    router.registerNative("plugins.discover", [ctx](const json& args) {
        if (!ctx || !ctx->pluginManager) return;
        std::string dir = args.value("dir", "");
        if (!dir.empty()) ctx->pluginManager->discover(dir);
    });

    // --- plugins.activate: Activate a plugin ---
    // --- plugins.activate: Bir eklentiyi etkinlestir ---
    router.registerNative("plugins.activate", [ctx](const json& args) {
        if (!ctx || !ctx->pluginManager) return;
        std::string name = args.value("name", "");
        if (!name.empty()) ctx->pluginManager->activate(name);
    });

    // --- plugins.deactivate: Deactivate a plugin ---
    // --- plugins.deactivate: Bir eklentiyi devre disi birak ---
    router.registerNative("plugins.deactivate", [ctx](const json& args) {
        if (!ctx || !ctx->pluginManager) return;
        std::string name = args.value("name", "");
        if (!name.empty()) ctx->pluginManager->deactivate(name);
    });

    // --- chars.removeWordChar: Remove character from word class ---
    // --- chars.removeWordChar: Kelime sinifindan karakter kaldir ---
    router.registerNative("chars.removeWordChar", [ctx](const json& args) {
        if (!ctx || !ctx->charClassifier) return;
        std::string ch = args.value("char", "");
        if (!ch.empty()) ctx->charClassifier->removeWordChar(ch[0]);
    });

    // --- process.shutdownAll: Terminate all processes ---
    // --- process.shutdownAll: Tum surecleri sonlandir ---
    router.registerNative("process.shutdownAll", [ctx](const json&) {
        if (!ctx || !ctx->processManager) return;
        ctx->processManager->shutdownAll();
    });

    // --- treesitter.editAndReparse: Incremental reparse after edit ---
    // --- treesitter.editAndReparse: Duzenleme sonrasi artimsal yeniden ayristir ---
    router.registerNative("treesitter.editAndReparse", [ctx](const json& args) {
        if (!ctx || !ctx->treeSitter) return;
        int sl = args.value("startLine", 0), sc = args.value("startCol", 0);
        int oel = args.value("oldEndLine", 0), oec = args.value("oldEndCol", 0);
        int nel = args.value("newEndLine", 0), nec = args.value("newEndCol", 0);
        std::string src = args.value("source", "");
        ctx->treeSitter->editAndReparse(sl, sc, oel, oec, nel, nec, src);
    });

    // ========================================================================
    // ADDITIONAL QUERY COMMANDS — Filling Tier 1 API gaps
    // EK SORGU KOMUTLARI — Tier 1 API boslukalarini doldurma
    // ========================================================================

    // --- buffer.getLine: Get a single line content ---
    // --- buffer.getLine: Tek bir satir icerigini al ---
    router.registerQuery("buffer.getLine", [ctx](const json& args) -> json {
        if (!ctx || !ctx->buffers) return "";
        int line = args.value("line", -1);
        if (line < 0) line = ctx->buffers->active().getCursor().getLine();
        auto& buf = ctx->buffers->active().getBuffer();
        if (line < 0 || line >= buf.lineCount()) return "";
        return buf.getLine(line);
    });

    // --- buffer.lineCount: Get total line count ---
    // --- buffer.lineCount: Toplam satir sayisini al ---
    router.registerQuery("buffer.lineCount", [ctx](const json&) -> json {
        if (!ctx || !ctx->buffers) return 0;
        return ctx->buffers->active().getBuffer().lineCount();
    });

    // --- buffer.columnCount: Get column count of a line ---
    // --- buffer.columnCount: Bir satirin sutun sayisini al ---
    router.registerQuery("buffer.columnCount", [ctx](const json& args) -> json {
        if (!ctx || !ctx->buffers) return 0;
        int line = args.value("line", -1);
        if (line < 0) line = ctx->buffers->active().getCursor().getLine();
        auto& buf = ctx->buffers->active().getBuffer();
        if (line < 0 || line >= buf.lineCount()) return 0;
        return buf.columnCount(line);
    });

    // --- buffer.isValidPos: Check if position is valid ---
    // --- buffer.isValidPos: Konumun gecerli olup olmadigini kontrol et ---
    router.registerQuery("buffer.isValidPos", [ctx](const json& args) -> json {
        if (!ctx || !ctx->buffers) return false;
        int line = args.value("line", -1), col = args.value("col", -1);
        return ctx->buffers->active().getBuffer().isValidPos(line, col);
    });

    // --- buffers.count: Get open buffer count ---
    // --- buffers.count: Acik buffer sayisini al ---
    router.registerQuery("buffers.count", [ctx](const json&) -> json {
        if (!ctx || !ctx->buffers) return 0;
        return (int)ctx->buffers->count();
    });

    // --- buffers.activeIndex: Get active buffer index ---
    // --- buffers.activeIndex: Aktif buffer indeksini al ---
    router.registerQuery("buffers.activeIndex", [ctx](const json&) -> json {
        if (!ctx || !ctx->buffers) return 0;
        return (int)ctx->buffers->activeIndex();
    });

    // --- buffers.findByPath: Find buffer by file path ---
    // --- buffers.findByPath: Dosya yoluyla buffer bul ---
    router.registerQuery("buffers.findByPath", [ctx](const json& args) -> json {
        if (!ctx || !ctx->buffers) return -1;
        std::string path = args.value("path", "");
        if (path.empty()) return -1;
        auto idx = ctx->buffers->findByPath(path);
        return idx.has_value() ? (int)idx.value() : -1;
    });

    // --- buffers.titleOf: Get buffer title at index ---
    // --- buffers.titleOf: Indeksteki buffer basligini al ---
    router.registerQuery("buffers.titleOf", [ctx](const json& args) -> json {
        if (!ctx || !ctx->buffers) return "";
        int index = args.value("index", -1);
        if (index < 0 || (size_t)index >= ctx->buffers->count()) return "";
        return ctx->buffers->titleOf(static_cast<size_t>(index));
    });

    // --- cursor.getPosition: Get cursor position ---
    // --- cursor.getPosition: Imlec konumunu al ---
    router.registerQuery("cursor.getPosition", [ctx](const json&) -> json {
        if (!ctx || !ctx->buffers) return nullptr;
        auto& cur = ctx->buffers->active().getCursor();
        return json({{"line", cur.getLine()}, {"col", cur.getCol()}});
    });

    // --- mode.get: Get current editing mode ---
    // --- mode.get: Mevcut duzenleme modunu al ---
    router.registerQuery("mode.get", [ctx](const json&) -> json {
        if (!ctx || !ctx->buffers) return "normal";
        auto m = ctx->buffers->active().getMode();
        switch (m) {
            case EditMode::Insert: return "insert";
            case EditMode::Visual: return "visual";
            default: return "normal";
        }
    });

    // --- buffer.isModified: Check if buffer has unsaved changes ---
    // --- buffer.isModified: Bufferin kaydedilmemis degisiklikleri olup olmadigini kontrol et ---
    router.registerQuery("buffer.isModified", [ctx](const json&) -> json {
        if (!ctx || !ctx->buffers) return false;
        return ctx->buffers->active().isModified();
    });

    // --- buffer.getFilePath: Get active buffer file path ---
    // --- buffer.getFilePath: Aktif buffer dosya yolunu al ---
    router.registerQuery("buffer.getFilePath", [ctx](const json&) -> json {
        if (!ctx || !ctx->buffers) return "";
        return ctx->buffers->active().getFilePath();
    });

    // --- undo.branchCount: Get undo branch count ---
    // --- undo.branchCount: Geri alma dal sayisini al ---
    router.registerQuery("undo.branchCount", [ctx](const json&) -> json {
        if (!ctx || !ctx->buffers) return 0;
        return ctx->buffers->active().getUndo().branchCount();
    });

    // --- undo.currentBranch: Get current undo branch index ---
    // --- undo.currentBranch: Mevcut geri alma dal indeksini al ---
    router.registerQuery("undo.currentBranch", [ctx](const json&) -> json {
        if (!ctx || !ctx->buffers) return -1;
        return ctx->buffers->active().getUndo().currentBranch();
    });

    // --- undo.inGroup: Check if inside undo group ---
    // --- undo.inGroup: Geri alma grubu icinde olup olmadigini kontrol et ---
    router.registerQuery("undo.inGroup", [ctx](const json&) -> json {
        if (!ctx || !ctx->buffers) return false;
        return ctx->buffers->active().getUndo().inGroup();
    });

    // --- folds.isLineHidden: Check if line is hidden by fold ---
    // --- folds.isLineHidden: Satirin katlama ile gizlenip gizlenmedigini kontrol et ---
    router.registerQuery("folds.isLineHidden", [ctx](const json& args) -> json {
        if (!ctx || !ctx->foldManager) return false;
        int line = args.value("line", -1);
        return line >= 0 && ctx->foldManager->isLineHidden(line);
    });

    // --- folds.visibleLineCount: Get visible line count after folds ---
    // --- folds.visibleLineCount: Katlamalardan sonra gorunen satir sayisini al ---
    router.registerQuery("folds.visibleLineCount", [ctx](const json& args) -> json {
        if (!ctx || !ctx->foldManager || !ctx->buffers) return 0;
        int total = args.value("total", ctx->buffers->active().getBuffer().lineCount());
        return ctx->foldManager->visibleLineCount(total);
    });

    // --- macro.isRecording: Check if macro recording is active ---
    // --- macro.isRecording: Makro kaydinin aktif olup olmadigini kontrol et ---
    router.registerQuery("macro.isRecording", [ctx](const json&) -> json {
        if (!ctx || !ctx->macroRecorder) return false;
        return ctx->macroRecorder->isRecording();
    });

    // --- macro.recordingRegister: Get current recording register ---
    // --- macro.recordingRegister: Mevcut kayit registerini al ---
    router.registerQuery("macro.recordingRegister", [ctx](const json&) -> json {
        if (!ctx || !ctx->macroRecorder || !ctx->macroRecorder->isRecording()) return "";
        return ctx->macroRecorder->recordingRegister();
    });

    // --- keymap.currentPrefix: Get current prefix state ---
    // --- keymap.currentPrefix: Mevcut onek durumunu al ---
    router.registerQuery("keymap.currentPrefix", [ctx](const json&) -> json {
        if (!ctx || !ctx->keymapManager) return "";
        return ctx->keymapManager->currentPrefix();
    });

    // --- keymap.hasPendingPrefix: Check if prefix state is pending ---
    // --- keymap.hasPendingPrefix: Onek durumunun beklemede olup olmadigini kontrol et ---
    router.registerQuery("keymap.hasPendingPrefix", [ctx](const json&) -> json {
        if (!ctx || !ctx->keymapManager) return false;
        return ctx->keymapManager->hasPendingPrefix();
    });

    // --- multicursor.isActive: Check if multi-cursor is active ---
    // --- multicursor.isActive: Coklu imlecin aktif olup olmadigini kontrol et ---
    router.registerQuery("multicursor.isActive", [ctx](const json&) -> json {
        if (!ctx || !ctx->multiCursor) return false;
        return ctx->multiCursor->isActive();
    });

    // --- selection.getAnchor: Get selection anchor position ---
    // --- selection.getAnchor: Secim baglama konumunu al ---
    router.registerQuery("selection.getAnchor", [ctx](const json&) -> json {
        if (!ctx || !ctx->buffers) return nullptr;
        auto& sel = ctx->buffers->active().getSelection();
        if (!sel.isActive()) return nullptr;
        return json({{"line", sel.anchorLine()}, {"col", sel.anchorCol()}});
    });

    // --- search.countMatches: Count all matches of pattern ---
    // --- search.countMatches: Kalibin tum eslesmelerini say ---
    router.registerQuery("search.countMatches", [ctx](const json& args) -> json {
        if (!ctx || !ctx->buffers || !ctx->searchEngine) return 0;
        std::string pattern = args.value("pattern", "");
        if (pattern.empty()) return 0;
        SearchOptions opts;
        opts.caseSensitive = args.value("caseSensitive", true);
        opts.regex = args.value("regex", false);
        return ctx->searchEngine->countMatches(ctx->buffers->active().getBuffer(), pattern, opts);
    });

    // --- search.lastPattern: Get last search pattern ---
    // --- search.lastPattern: Son arama kalibini al ---
    router.registerQuery("search.lastPattern", [ctx](const json&) -> json {
        if (!ctx || !ctx->searchEngine) return "";
        return ctx->searchEngine->lastPattern();
    });

    // --- file.exists: Check if file exists ---
    // --- file.exists: Dosyanin var olup olmadigini kontrol et ---
    router.registerQuery("file.exists", [ctx](const json& args) -> json {
        std::string path = args.value("path", "");
        return !path.empty() && FileSystem::exists(path);
    });

    // --- file.isReadable: Check if file is readable ---
    // --- file.isReadable: Dosyanin okunabilir olup olmadigini kontrol et ---
    router.registerQuery("file.isReadable", [ctx](const json& args) -> json {
        std::string path = args.value("path", "");
        return !path.empty() && FileSystem::isReadable(path);
    });

    // --- file.isWritable: Check if file is writable ---
    // --- file.isWritable: Dosyanin yazilabilir olup olmadigini kontrol et ---
    router.registerQuery("file.isWritable", [ctx](const json& args) -> json {
        std::string path = args.value("path", "");
        return !path.empty() && FileSystem::isWritable(path);
    });

    // --- file.info: Get file info (size, modified time) ---
    // --- file.info: Dosya bilgisini al (boyut, degistirilme zamani) ---
    router.registerQuery("file.info", [ctx](const json& args) -> json {
        std::string path = args.value("path", "");
        if (path.empty()) return nullptr;
        auto info = FileSystem::getFileInfo(path);
        if (!info) return nullptr;
        return json({{"path", info->path}, {"size", info->size}, {"modified", info->modified}});
    });

    // --- file.loadText: Load file as text string ---
    // --- file.loadText: Dosyayi metin olarak yukle ---
    router.registerQuery("file.loadText", [ctx](const json& args) -> json {
        std::string path = args.value("path", "");
        if (path.empty()) return nullptr;
        auto text = FileSystem::loadTextFile(path);
        return text.has_value() ? json(text.value()) : json(nullptr);
    });

    // --- help.listTopics: List all help topics ---
    // --- help.listTopics: Tum yardim konularini listele ---
    router.registerQuery("help.listTopics", [ctx](const json&) -> json {
        if (!ctx || !ctx->helpSystem) return json::array();
        auto topics = ctx->helpSystem->listTopics();
        json arr = json::array();
        for (auto* t : topics) {
            arr.push_back({{"id", t->id}, {"title", t->title}, {"tags", t->tags}});
        }
        return arr;
    });

    // --- help.getTopic: Get a help topic by ID ---
    // --- help.getTopic: ID ile yardim konusunu al ---
    router.registerQuery("help.getTopic", [ctx](const json& args) -> json {
        if (!ctx || !ctx->helpSystem) return nullptr;
        std::string id = args.value("id", "");
        if (id.empty()) return nullptr;
        auto* t = ctx->helpSystem->getTopic(id);
        if (!t) return nullptr;
        return json({{"id", t->id}, {"title", t->title}, {"content", t->content}, {"tags", t->tags}});
    });

    // --- help.search: Search help topics ---
    // --- help.search: Yardim konularini ara ---
    router.registerQuery("help.search", [ctx](const json& args) -> json {
        if (!ctx || !ctx->helpSystem) return json::array();
        std::string query = args.value("query", "");
        if (query.empty()) return json::array();
        auto results = ctx->helpSystem->search(query);
        json arr = json::array();
        for (auto* t : results) {
            arr.push_back({{"id", t->id}, {"title", t->title}, {"tags", t->tags}});
        }
        return arr;
    });

    // --- autosave.hasExternalChange: Check if file changed externally ---
    // --- autosave.hasExternalChange: Dosyanin harici olarak degisip degismedigini kontrol et ---
    router.registerQuery("autosave.hasExternalChange", [ctx](const json& args) -> json {
        if (!ctx || !ctx->autoSave) return false;
        std::string path = args.value("path", "");
        return !path.empty() && ctx->autoSave->hasExternalChange(path);
    });

    // --- treesitter.hasTree: Check if syntax tree exists ---
    // --- treesitter.hasTree: Sozdizimi agacinin var olup olmadigini kontrol et ---
    router.registerQuery("treesitter.hasTree", [ctx](const json&) -> json {
        if (!ctx || !ctx->treeSitter) return false;
        return ctx->treeSitter->hasTree();
    });

    // --- treesitter.rootNode: Get root node of syntax tree ---
    // --- treesitter.rootNode: Sozdizimi agacinin kok dugumunu al ---
    router.registerQuery("treesitter.rootNode", [ctx](const json&) -> json {
        if (!ctx || !ctx->treeSitter || !ctx->treeSitter->hasTree()) return nullptr;
        auto node = ctx->treeSitter->rootNode();
        return json({{"type", node.type}, {"startLine", node.startLine}, {"startCol", node.startCol},
                     {"endLine", node.endLine}, {"endCol", node.endCol}, {"isNamed", node.isNamed}});
    });

    // --- treesitter.namedNodeAt: Get named node at position ---
    // --- treesitter.namedNodeAt: Konumdaki adli dugumu al ---
    router.registerQuery("treesitter.namedNodeAt", [ctx](const json& args) -> json {
        if (!ctx || !ctx->treeSitter || !ctx->treeSitter->hasTree()) return nullptr;
        int line = args.value("line", 0), col = args.value("col", 0);
        auto node = ctx->treeSitter->namedNodeAt(line, col);
        return json({{"type", node.type}, {"startLine", node.startLine}, {"startCol", node.startCol},
                     {"endLine", node.endLine}, {"endCol", node.endCol}, {"isNamed", node.isNamed}});
    });

    // --- treesitter.errors: Get syntax errors ---
    // --- treesitter.errors: Sozdizimi hatalarini al ---
    router.registerQuery("treesitter.errors", [ctx](const json&) -> json {
        if (!ctx || !ctx->treeSitter || !ctx->treeSitter->hasTree()) return json::array();
        auto errors = ctx->treeSitter->errors();
        json arr = json::array();
        for (auto& e : errors) {
            arr.push_back({{"type", e.type}, {"startLine", e.startLine}, {"startCol", e.startCol},
                           {"endLine", e.endLine}, {"endCol", e.endCol}});
        }
        return arr;
    });

    // --- chars.isWhitespace: Check if character is whitespace ---
    // --- chars.isWhitespace: Karakterin bosluk olup olmadigini kontrol et ---
    router.registerQuery("chars.isWhitespace", [ctx](const json& args) -> json {
        if (!ctx || !ctx->charClassifier) return false;
        std::string ch = args.value("char", "");
        return !ch.empty() && ctx->charClassifier->isWhitespace(ch[0]);
    });

    // --- chars.isBracket: Check if character is a bracket ---
    // --- chars.isBracket: Karakterin parantez olup olmadigini kontrol et ---
    router.registerQuery("chars.isBracket", [ctx](const json& args) -> json {
        if (!ctx || !ctx->charClassifier) return false;
        std::string ch = args.value("char", "");
        return !ch.empty() && ctx->charClassifier->isBracket(ch[0]);
    });

    // --- chars.bracketPairs: Get all bracket pairs ---
    // --- chars.bracketPairs: Tum parantez ciftlerini al ---
    router.registerQuery("chars.bracketPairs", [ctx](const json&) -> json {
        if (!ctx || !ctx->charClassifier) return json::array();
        auto pairs = ctx->charClassifier->bracketPairs();
        json arr = json::array();
        for (auto& [open, close] : pairs) {
            arr.push_back({{"open", std::string(1, open)}, {"close", std::string(1, close)}});
        }
        return arr;
    });

    // --- indent.forNewLine: Calculate indent for new line ---
    // --- indent.forNewLine: Yeni satir icin girinti hesapla ---
    router.registerQuery("indent.forNewLine", [ctx](const json& args) -> json {
        if (!ctx || !ctx->indentEngine || !ctx->buffers) return "";
        auto& buf = ctx->buffers->active().getBuffer();
        int line = args.value("line", -1);
        if (line < 0) line = ctx->buffers->active().getCursor().getLine();
        if (line < 0 || line >= buf.lineCount()) return "";
        auto r = ctx->indentEngine->indentForNewLine(buf, line);
        return json({{"level", r.level}, {"indentString", r.indentString}});
    });

    // --- indent.forLine: Calculate correct indent for a line ---
    // --- indent.forLine: Bir satir icin dogru girintiyi hesapla ---
    router.registerQuery("indent.forLine", [ctx](const json& args) -> json {
        if (!ctx || !ctx->indentEngine || !ctx->buffers) return "";
        auto& buf = ctx->buffers->active().getBuffer();
        int line = args.value("line", -1);
        if (line < 0) line = ctx->buffers->active().getCursor().getLine();
        if (line < 0 || line >= buf.lineCount()) return "";
        auto r = ctx->indentEngine->indentForLine(buf, line);
        return json({{"level", r.level}, {"indentString", r.indentString}});
    });

    // --- indent.getLevel: Get indent level of a line ---
    // --- indent.getLevel: Bir satirin girinti seviyesini al ---
    router.registerQuery("indent.getLevel", [ctx](const json& args) -> json {
        if (!ctx || !ctx->indentEngine || !ctx->buffers) return 0;
        auto& buf = ctx->buffers->active().getBuffer();
        int line = args.value("line", -1);
        if (line < 0) line = ctx->buffers->active().getCursor().getLine();
        if (line < 0 || line >= buf.lineCount()) return 0;
        return ctx->indentEngine->getIndentLevel(buf.getLine(line));
    });

    // --- completion.maxResults: Get max completion results setting ---
    // --- completion.maxResults: Maks tamamlama sonucu ayarini al ---
    router.registerQuery("completion.maxResults", [ctx](const json&) -> json {
        if (!ctx || !ctx->completionEngine) return 50;
        return ctx->completionEngine->maxResults();
    });

    // --- extmarks.getOnLine: Get extmarks on a specific line ---
    // --- extmarks.getOnLine: Belirli bir satirdaki extmarklari al ---
    router.registerQuery("extmarks.getOnLine", [ctx](const json& args) -> json {
        if (!ctx || !ctx->extmarkManager) return json::array();
        int line = args.value("line", -1);
        std::string ns = args.value("namespace", "");
        if (line < 0) return json::array();
        auto marks = ctx->extmarkManager->getInRange(line, line, ns);
        json arr = json::array();
        for (auto* em : marks) {
            arr.push_back({{"id", em->id}, {"namespace", em->ns}, {"startLine", em->startLine},
                           {"startCol", em->startCol}, {"endLine", em->endLine}, {"endCol", em->endCol},
                           {"type", em->type}, {"data", em->data}});
        }
        return arr;
    });

    // --- windows.getWindow: Get window info by ID ---
    // --- windows.getWindow: ID ile pencere bilgisini al ---
    router.registerQuery("windows.getWindow", [ctx](const json& args) -> json {
        if (!ctx || !ctx->windowManager) return nullptr;
        int id = args.value("id", -1);
        if (id < 0) return nullptr;
        auto* w = ctx->windowManager->getWindow(id);
        if (!w) return nullptr;
        return json({{"id", w->id}, {"bufferIndex", w->bufferIndex}, {"scrollTop", w->scrollTop},
                     {"cursorLine", w->cursorLine}, {"cursorCol", w->cursorCol},
                     {"width", w->width}, {"height", w->height}});
    });

    // --- encoding.isASCII: Check if text is pure ASCII ---
    // --- encoding.isASCII: Metnin saf ASCII olup olmadigini kontrol et ---
    router.registerQuery("encoding.isASCII", [ctx](const json& args) -> json {
        if (!ctx || !ctx->encodingDetector) return false;
        std::string text = args.value("text", "");
        return ctx->encodingDetector->isASCII(
            reinterpret_cast<const uint8_t*>(text.data()), text.size());
    });

    // --- process.getProcess: Get process info by ID ---
    // --- process.getProcess: ID ile surec bilgisini al ---
    router.registerQuery("process.getProcess", [ctx](const json& args) -> json {
        if (!ctx || !ctx->processManager) return nullptr;
        int id = args.value("id", -1);
        if (id < 0) return nullptr;
        auto procs = ctx->processManager->list();
        for (auto& p : procs) {
            if (p.id == id) {
                return json({{"id", p.id}, {"pid", p.pid},
                             {"running", p.running}, {"exitCode", p.exitCode}});
            }
        }
        return nullptr;
    });

    // ======================================================================
    // BufferOptions commands — per-buffer and global option management
    // BufferOptions komutlari — buffer-bazli ve global secenek yonetimi
    // ======================================================================

    // --- options.setDefault: Set a global default option value ---
    // --- options.setDefault: Global varsayilan secenek degerini ayarla ---
    router.registerNative("options.setDefault", [ctx](const json& args) {
        if (!ctx || !ctx->bufferOptions) return;
        std::string key = args.value("key", "");
        if (key.empty()) return;
        if (!args.contains("value")) return;
        const auto& val = args["value"];
        // Detect type: bool, int, double, string
        // Tip algila: bool, int, double, string
        if (val.is_boolean()) {
            ctx->bufferOptions->setDefault(key, val.get<bool>());
        } else if (val.is_number_integer()) {
            ctx->bufferOptions->setDefault(key, val.get<int>());
        } else if (val.is_number()) {
            ctx->bufferOptions->setDefault(key, val.get<double>());
        } else {
            ctx->bufferOptions->setDefault(key, val.get<std::string>());
        }
    });

    // --- options.setLocal: Set a buffer-local option (overrides global default) ---
    // --- options.setLocal: Buffer-yerel secenegi ayarla (global varsayilani gecersiz kilar) ---
    router.registerNative("options.setLocal", [ctx](const json& args) {
        if (!ctx || !ctx->bufferOptions) return;
        int bufferId = args.value("bufferId", -1);
        std::string key = args.value("key", "");
        if (bufferId < 0 || key.empty()) return;
        if (!args.contains("value")) return;
        const auto& val = args["value"];
        // Detect type: bool, int, double, string
        // Tip algila: bool, int, double, string
        if (val.is_boolean()) {
            ctx->bufferOptions->setLocal(bufferId, key, val.get<bool>());
        } else if (val.is_number_integer()) {
            ctx->bufferOptions->setLocal(bufferId, key, val.get<int>());
        } else if (val.is_number()) {
            ctx->bufferOptions->setLocal(bufferId, key, val.get<double>());
        } else {
            ctx->bufferOptions->setLocal(bufferId, key, val.get<std::string>());
        }
    });

    // --- options.removeLocal: Remove a buffer-local option (falls back to global) ---
    // --- options.removeLocal: Buffer-yerel secenegi kaldir (global varsayilana doner) ---
    router.registerNative("options.removeLocal", [ctx](const json& args) {
        if (!ctx || !ctx->bufferOptions) return;
        int bufferId = args.value("bufferId", -1);
        std::string key = args.value("key", "");
        if (bufferId < 0 || key.empty()) return;
        ctx->bufferOptions->removeLocal(bufferId, key);
    });

    // --- options.clearBuffer: Clear all local options for a buffer ---
    // --- options.clearBuffer: Bir buffer icin tum yerel secenekleri temizle ---
    router.registerNative("options.clearBuffer", [ctx](const json& args) {
        if (!ctx || !ctx->bufferOptions) return;
        int bufferId = args.value("bufferId", -1);
        if (bufferId < 0) return;
        ctx->bufferOptions->clearBuffer(bufferId);
    });

    // --- extmarks.setWithVirtText: Set an extmark with virtual text ---
    // --- extmarks.setWithVirtText: Sanal metinli bir extmark ayarla ---
    router.registerNative("extmarks.setWithVirtText", [ctx](const json& args) {
        if (!ctx || !ctx->extmarkManager) return;
        std::string ns = args.value("namespace", "default");
        int sl = args.value("startLine", 0), sc = args.value("startCol", 0);
        int el = args.value("endLine", sl), ec = args.value("endCol", sc);
        std::string virtText = args.value("virtText", "");
        std::string virtPosStr = args.value("virtTextPos", "none");
        std::string virtStyle = args.value("virtStyle", "");
        std::string type = args.value("type", "");
        std::string data = args.value("data", "");
        // Convert string to VirtTextPos enum
        // String'i VirtTextPos enum'una donustur
        VirtTextPos vp = VirtTextPos::None;
        if (virtPosStr == "eol") vp = VirtTextPos::EOL;
        else if (virtPosStr == "inline") vp = VirtTextPos::Inline;
        else if (virtPosStr == "overlay") vp = VirtTextPos::Overlay;
        else if (virtPosStr == "rightAlign") vp = VirtTextPos::RightAlign;
        ctx->extmarkManager->setWithVirtText(ns, sl, sc, el, ec, virtText, vp, virtStyle, type, data);
    });

    // ======================================================================
    // BufferOptions queries — option value retrieval
    // BufferOptions sorgulari — secenek degeri alma
    // ======================================================================

    // --- options.getDefault: Get a global default option value ---
    // --- options.getDefault: Global varsayilan secenek degerini al ---
    router.registerQuery("options.getDefault", [ctx](const json& args) -> json {
        if (!ctx || !ctx->bufferOptions) return nullptr;
        std::string key = args.value("key", "");
        if (key.empty()) return nullptr;
        auto opt = ctx->bufferOptions->getDefault(key);
        if (!opt.has_value()) return nullptr;
        // Convert OptionValue variant to json
        // OptionValue variant'ini json'a donustur
        return std::visit([](const auto& v) -> json { return v; }, opt.value());
    });

    // --- options.get: Get effective option value (local > global) ---
    // --- options.get: Gecerli secenek degerini al (yerel > global) ---
    router.registerQuery("options.get", [ctx](const json& args) -> json {
        if (!ctx || !ctx->bufferOptions) return nullptr;
        int bufferId = args.value("bufferId", -1);
        std::string key = args.value("key", "");
        if (bufferId < 0 || key.empty()) return nullptr;
        auto opt = ctx->bufferOptions->get(bufferId, key);
        if (!opt.has_value()) return nullptr;
        // Convert OptionValue variant to json
        // OptionValue variant'ini json'a donustur
        return std::visit([](const auto& v) -> json { return v; }, opt.value());
    });

    // --- options.hasLocal: Check if buffer has a local override ---
    // --- options.hasLocal: Buffer'in yerel gecersiz kilma degeri olup olmadigini kontrol et ---
    router.registerQuery("options.hasLocal", [ctx](const json& args) -> json {
        if (!ctx || !ctx->bufferOptions) return false;
        int bufferId = args.value("bufferId", -1);
        std::string key = args.value("key", "");
        if (bufferId < 0 || key.empty()) return false;
        return ctx->bufferOptions->hasLocal(bufferId, key);
    });

    // --- options.listKeys: List all option keys for a buffer ---
    // --- options.listKeys: Bir buffer icin tum secenek anahtarlarini listele ---
    router.registerQuery("options.listKeys", [ctx](const json& args) -> json {
        if (!ctx || !ctx->bufferOptions) return json::array();
        int bufferId = args.value("bufferId", -1);
        if (bufferId < 0) return json::array();
        auto keys = ctx->bufferOptions->listKeys(bufferId);
        return json(keys);
    });

    // --- options.listLocalKeys: List buffer-local override keys ---
    // --- options.listLocalKeys: Buffer-yerel gecersiz kilma anahtarlarini listele ---
    router.registerQuery("options.listLocalKeys", [ctx](const json& args) -> json {
        if (!ctx || !ctx->bufferOptions) return json::array();
        int bufferId = args.value("bufferId", -1);
        if (bufferId < 0) return json::array();
        auto keys = ctx->bufferOptions->listLocalKeys(bufferId);
        return json(keys);
    });

    // --- options.listDefaultKeys: List all global default option keys ---
    // --- options.listDefaultKeys: Tum global varsayilan secenek anahtarlarini listele ---
    router.registerQuery("options.listDefaultKeys", [ctx](const json&) -> json {
        if (!ctx || !ctx->bufferOptions) return json::array();
        auto keys = ctx->bufferOptions->listDefaultKeys();
        return json(keys);
    });

    // --- options.getInt: Get option as int with fallback ---
    // --- options.getInt: Secenegi int olarak al (fallback ile) ---
    router.registerQuery("options.getInt", [ctx](const json& args) -> json {
        if (!ctx || !ctx->bufferOptions) return 0;
        int bufferId = args.value("bufferId", -1);
        std::string key = args.value("key", "");
        int fallback = args.value("fallback", 0);
        if (bufferId < 0 || key.empty()) return fallback;
        return ctx->bufferOptions->getInt(bufferId, key, fallback);
    });

    // --- options.getBool: Get option as bool with fallback ---
    // --- options.getBool: Secenegi bool olarak al (fallback ile) ---
    router.registerQuery("options.getBool", [ctx](const json& args) -> json {
        if (!ctx || !ctx->bufferOptions) return false;
        int bufferId = args.value("bufferId", -1);
        std::string key = args.value("key", "");
        bool fallback = args.value("fallback", false);
        if (bufferId < 0 || key.empty()) return fallback;
        return ctx->bufferOptions->getBool(bufferId, key, fallback);
    });

    // --- options.getString: Get option as string with fallback ---
    // --- options.getString: Secenegi string olarak al (fallback ile) ---
    router.registerQuery("options.getString", [ctx](const json& args) -> json {
        if (!ctx || !ctx->bufferOptions) return "";
        int bufferId = args.value("bufferId", -1);
        std::string key = args.value("key", "");
        std::string fallback = args.value("fallback", "");
        if (bufferId < 0 || key.empty()) return fallback;
        return ctx->bufferOptions->getString(bufferId, key, fallback);
    });

    LOG_INFO("[Command] Core commands registered: ~130 mutations + ~115 queries.");
}
