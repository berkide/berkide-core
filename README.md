<div align="center">

# BerkIDE

### No impositions.

No UI imposed. No language imposed. No workflow imposed. No rules imposed.

**C++ core. V8 JavaScript runtime. Headless server. You decide the rest.**

</div>

---

## What Makes BerkIDE Different

- **Headless by design** — No built-in UI. Your terminal, your React app, your mobile client — all first-class citizens via HTTP/WS.
- **Dual API** — Talk to core directly (HTTP/WS, any language) or through V8 JavaScript (like Emacs Lisp). Your choice.
- **V8 = Your Lisp** — JavaScript is the extension language. ES6 modules, hot-reload, Chrome DevTools debugging, WASM, worker threads.
- **Open core** — Want Python middleware instead of JS? Go ahead. Core API is Tier 1 — no V8 required.
- **Piece Table + Tree Undo** — Copy-on-write buffer engine + branching undo history. Never lose an edit.
- **True parallelism** — Worker threads with isolated V8 isolates. No GIL. No hacks.
- **WASM native** — Load Rust/C/Go compiled to WebAssembly directly in V8. No Node.js. No Electron.
- **28 subsystems, zero bloat** — Every feature is a wired C++ subsystem. No scripting overhead for core operations.

---

## Architecture — 3 Layers + Dual API

```
  ┌─────────────────────────────────────────────────────────────────────┐
  │                    LAYER 3 — UI CLIENTS                            │
  │         React Web  │  Terminal TUI  │  Mobile  │  Any UI           │
  │         Renders, draws, captures input (syntax colors, popups)     │
  ├────────────────────────────┬────────────────────────────────────────┤
  │                            │ HTTP REST + WebSocket                  │
  │                    LAYER 2 — JS MIDDLEWARE (V8)                     │
  │     git.js │ lsp.js │ vim-mode.js │ theme.js │ snippets.js       │
  │     Workers (isolated V8) │ WASM modules │ Chrome DevTools         │
  ├────────────────────────────┼────────────────────────────────────────┤
  │                            │ 34 V8 Bindings (editor.*)              │
  │                    LAYER 1 — C++ CORE ENGINE                       │
  │     Buffer(PieceTable) │ Cursor │ Undo(Tree) │ EventBus            │
  │     Search │ Diff(Myers) │ MultiCursor │ WindowManager             │
  │     TreeSitter │ CharClassifier │ IndentEngine │ 28 subsystems     │
  ├─────────────────────────────────────────────────────────────────────┤
  │     HTTP REST (cpp-httplib) │ WebSocket (IXWebSocket) │ TLS │ Auth │
  └─────────────────────────────────────────────────────────────────────┘

  TIER 1 API: [Any Language] ──HTTP/WS──► [C++ Core]     (direct access)
  TIER 2 API: [JS Plugin]   ──editor.*──► [V8 Binding] ──► [C++ Core]
```

> **Core Rule:** If it reads or modifies buffer/cursor data → **C++ core**.
> If it interprets keys or renders UI → **JS plugin / UI client**.

---

## What Goes Where — Layer Decision Table

| Feature | Layer | Why |
|---------|-------|-----|
| Buffer read/write | **Core C++** | Every keystroke, performance critical |
| Cursor movement | **Core C++** | Every keystroke |
| Undo/Redo | **Core C++** | Data structure, tree-based |
| File I/O | **Core C++** | OS syscall |
| Process spawn | **Core C++** | OS syscall (fork/CreateProcess) |
| Search/Replace (regex) | **Core C++** | Performance critical |
| Event system | **Core C++** | Thread-safe pub/sub |
| Encoding detection | **Core C++** | Byte-level analysis |
| Diff (Myers algorithm) | **Core C++** | Algorithm, performance |
| Tree-sitter parsing | **Core C++** | C library binding |
| LSP client | **JS Plugin** | JSON-RPC protocol, uses process.spawn() |
| Git integration | **JS Plugin** | Shell commands, output parsing |
| Formatter (prettier etc.) | **JS Plugin** | External tool invocation |
| Linter | **JS Plugin** | External tool or LSP |
| Snippet engine | **JS Plugin** | Template expand, cursor placement |
| Theme/color scheme | **JS Plugin** | Color defs, scope→color mapping |
| Keybinding modes (vim/emacs) | **JS Plugin** | Keymap + command dispatch |
| File explorer logic | **JS Plugin** | Directory listing, filtering |
| Project management | **JS Plugin** | Workspace config, build integration |
| Syntax coloring/rendering | **UI** | Render pipeline, screen drawing |
| Popup/menu display | **UI** | Widget rendering |
| Status bar | **UI** | Layout, render |
| Input capture | **UI** | Platform-specific key events |
| Scrollbar, line numbers | **UI** | Visual elements |
| Terminal emulator | **UI** | VT100/xterm rendering |

---

## Dual API — Your Language, Your Rules

### Tier 1 — Direct Core Access (HTTP/WS)
```
  [Python / Go / Rust / Any Language] ──HTTP REST + WebSocket──► [C++ Core]
```
Core exposes every subsystem via REST endpoints and WebSocket commands.
No V8 needed. Build your own middleware in any language.

### Tier 2 — JS Plugin API (V8 Bindings)
```
  [JS Plugin (.js)] ──editor.* API──► [V8 Bindings] ──► [C++ Core]
```
34 bindings on the global `editor` object. Full Emacs Lisp-level power.
All `.js` files are ES6 modules with `import`/`export`. Hot-reload, Chrome DevTools, Workers, WASM.

### vs. Emacs
| | Emacs | BerkIDE |
|---|---|---|
| Core | C | C++ |
| Extension | Emacs Lisp (only option) | **JS (V8) + any language via HTTP/WS** |
| Core API access | Lisp only | **Tier 1 (HTTP/WS) + Tier 2 (JS)** |
| UI | Built into core | **Separate client process** |
| Debugging | \*Messages\* buffer | **Chrome DevTools (V8 Inspector)** |
| Parallelism | Single-threaded | **Worker isolates (true threads)** |
| WASM | No | **Native V8 WASM** |

### Why JS Is the First-Class Citizen

JavaScript is not just "one of the supported languages" — it's the **embedded extension language**, like Lisp in Emacs. V8 lives inside the editor process. This gives JS a fundamental advantage:

```
  OTHER LANGUAGES (Python, Go, Rust, etc.)
  ┌─────────────────────────────────────────────────────────┐
  │  Can only use: POST /api/command {"cmd":"...", "args":…} │
  │  163 commands/queries via HTTP/WS                        │
  │  Serialization overhead (JSON encode/decode each call)   │
  │  Network round-trip per operation                        │
  └─────────────────────────────────────────────────────────┘

  JAVASCRIPT (V8 — lives inside the editor process)
  ┌─────────────────────────────────────────────────────────┐
  │  Path A: editor.buffer.getLine(5)    ← DIRECT C++ call  │
  │          editor.cursor.moveUp()      ← No serialization  │
  │          editor.search.findAll(...)  ← No network hop    │
  │          34 namespaces, hundreds of methods               │
  │                                                           │
  │  Path B: editor.commands.exec("x")  ← Same as HTTP/WS   │
  │          (can also use the universal command pipeline)    │
  └─────────────────────────────────────────────────────────┘
```

**What this means in practice:**

- **JS plugins call C++ directly** — `editor.buffer.getLine(5)` is a V8 binding that calls `Buffer::getLine()` in C++ with zero serialization. No JSON, no HTTP, no network. Just a function pointer.
- **Other languages go through the command API** — `POST /api/command` serializes args to JSON, sends over HTTP/WS, deserializes, executes, serializes result, sends back. Works perfectly, but has overhead.
- **JS has a richer API** — V8 bindings expose 34 namespaces with fine-grained methods (`editor.buffer`, `editor.cursor`, `editor.chars`, `editor.diff`, etc.). The command API covers ~163 operations. Both are powerful, but JS has more granularity.
- **Performance-critical plugins should be JS** — A vim-mode plugin that runs on every keystroke? Write it in JS. A CI dashboard that polls build status every 30 seconds? Any language works fine.

This is exactly the Emacs model: Lisp lives inside Emacs and has direct access to everything. External tools talk to Emacs through a command interface. The difference: **Emacs has no external API at all** — BerkIDE gives every language a way in.

---

## V8 Superpowers — For Plugin Developers

- **ES6 Modules Everywhere** — All `.js` files load as ES6 modules. `import`/`export` works out of the box. No `.mjs` required (but supported).
- **Hot-Reload** — File watcher detects changes, auto-reloads your plugin. No restart.
- **Chrome DevTools** — `--inspect` flag, set breakpoints, profile, step through code.
- **Worker Threads** — Each worker gets its own V8 isolate. True parallelism, no shared state bugs.
- **WASM Native** — Load `.wasm` files compiled from Rust/C/Go. V8 runs them at near-native speed.
- **34 Bindings** — Every core subsystem is accessible: `editor.buffer`, `editor.cursor`, `editor.events`, `editor.process`, `editor.search`, `editor.diff`, `editor.treeSitter`, and 27 more.
- **npm Compatible** — Standard JavaScript. Bundle any npm package with a bundler and use it.
- **Command System** — Register JS commands, execute native or JS commands, build complex workflows.

---

## How It Works — Unified Command Pipeline

Every operation in BerkIDE — whether it comes from HTTP REST, WebSocket, or a JS plugin — flows through the **same command pipeline**. Zero duplication. One source of truth.

```
  ┌─────────────────┐   ┌──────────────────┐   ┌─────────────────┐
  │   HTTP REST      │   │   WebSocket      │   │   JS Plugin     │
  │   (any language) │   │   (real-time)    │   │   (V8 engine)   │
  │                  │   │                  │   │                 │
  │ POST /api/command│   │ {"cmd":"x",...}  │   │ editor.commands │
  │ POST /api/buffer │   │                  │   │   .exec("x")   │
  └────────┬─────────┘   └────────┬─────────┘   └───────┬─────────┘
           │                      │                     │
           └──────────────────────┼─────────────────────┘
                                  ▼
                   ┌──────────────────────────┐
                   │  V8Engine::dispatchCommand│
                   │  "Single entry point"     │
                   └────────────┬─────────────┘
                                │
                   ┌────────────▼─────────────┐
                   │     CommandRouter         │
                   │  1. Try native C++ (73)   │
                   │  2. Fallback to JS plugin │
                   └────────────┬─────────────┘
                                │
                   ┌────────────▼─────────────┐
                   │     C++ Core Engine       │
                   │  Buffer │ Cursor │ Undo   │
                   │  EventBus.emit() ──────────────► WS broadcast
                   └──────────────────────────┘       to all clients
```

### What this means:

- **Python/Go/Rust client?** → `POST /api/command {"cmd":"file.open","args":{"path":"x"}}` — same pipeline as JS
- **WebSocket client?** → `{"cmd":"file.open","args":{"path":"x"}}` — same pipeline
- **JS plugin?** → `editor.commands.exec("file.open", {path:"x"})` — same pipeline
- **All events fire correctly** — `EventBus` emits buffer/cursor/tab changes → all WS clients get real-time updates
- **No hardcoded shortcuts** — HTTP doesn't touch buffers directly, it always goes through CommandRouter

---

## Why BerkIDE?

| | Vim/Neovim | Emacs | VS Code | **BerkIDE** |
|---|---|---|---|---|
| Extension Language | VimL / Lua | Emacs Lisp | TypeScript (Node) | **JavaScript (V8)** |
| Architecture | Monolithic | Monolithic | Electron + Node | **Headless server + any client** |
| Buffer Engine | Gap buffer | Gap buffer | Piece tree | **Piece Table (COW)** |
| Undo Model | Linear | Linear | Linear | **Tree (branching)** |
| Multi-cursor | Plugin | Plugin | Built-in | **Built-in** |
| Plugin Debugging | printf | \*Messages\* | Separate Node | **Chrome DevTools (V8 Inspector)** |
| WASM Plugins | No | No | No (Node only) | **Native V8 WASM** |
| Worker Threads | No | No | Web Workers (limited) | **Isolated V8 per thread** |
| Diff/Merge | External | ediff | Built-in | **3-way merge (Myers)** |
| Remote Editing | SSH | TRAMP | SSH + Extension | **HTTP/WS + TLS** |

---

## Architecture

```
 ┌─────────────────────────────────────────────────────────┐
 │  UI Clients (connect via HTTP/WS)                       │
 │  React Web App  |  Terminal TUI  |  Mobile  |  Web      │
 ├─────────────────────────────────────────────────────────┤
 │  JS Plugin Layer (ES6 modules)                          │
 │  git.js | lsp.js | theme.js | vim-mode.js | ...         │
 ├─────────────────────────────────────────────────────────┤
 │  V8 Engine + 34 Bindings + Inspector + WASM + Workers   │
 │  editor.buffer | editor.cursor | editor.events | ...    │
 ├─────────────────────────────────────────────────────────┤
 │  C++ Core Engine (28 subsystems)                        │
 │  Buffer(PieceTable) | Cursor | Undo(Tree) | EventBus   │
 │  Search | Diff(Myers) | MultiCursor | WindowManager     │
 │  CharClassifier | IndentEngine | TreeSitter | ...       │
 ├─────────────────────────────────────────────────────────┤
 │  Servers + Auth + TLS                                   │
 │  HTTP REST API (cpp-httplib) | WebSocket (IXWebSocket)  │
 └─────────────────────────────────────────────────────────┘
```

### Core Rule

> If it reads or modifies buffer/cursor data = **C++ core**.
> If it interprets keys or renders UI = **JS plugin / client**.

---

## Features

### Text Engine
- **Piece Table** with copy-on-write — O(1) amortized inserts, memory-efficient for large files
- **Tree-based Undo/Redo** — branching history, never lose an edit (like Emacs undo-tree)
- **Multi-cursor editing** — independent cursors with per-cursor selection
- **Character classifier** — word boundary detection, bracket matching, configurable per language
- **Auto-indent engine** — core mechanism with JS plugin callback for language-specific rules
- **Selection system** — character-wise, line-wise, and block (column) selection
- **Search & Replace** — literal and regex, case/whole-word options, wrap-around
- **3-way diff/merge** — Myers algorithm with conflict detection
- **Code folding** — manual fold regions + tree-sitter/indent integration points
- **Text decorations (extmarks)** — attach metadata to buffer ranges, auto-adjusts on edits
- **Encoding detection** — UTF-8, UTF-8 BOM, UTF-16 LE/BE, UTF-32, ASCII, Latin-1

### Plugin System (V8 JavaScript)
- **All `.js` = ES6 modules** — `import`/`export` works in every `.js` file. No `.mjs` required.
- **Two plugin types** — Live plugins (like Emacs `.el`) and Built plugins (packaged, publishable)
- **init.js** — User config file like Emacs `init.el`, loaded first on startup
- **Hot-reload** — file watcher detects changes, auto-reloads plugins instantly
- **Plugin Manager** — `plugin.json` manifest, dependency topological sort, enable/disable lifecycle
- **npm ecosystem** — standard JS, use any npm package with a bundler (build output only)
- **34 JavaScript bindings** — every core subsystem exposed to JS
- **IIFE fallback** — `loadScriptAsIIFE()` method available for edge cases (not used by default)

### Unique Features (No Other Editor Has These)
- **V8 Inspector** — debug plugins with Chrome DevTools (`--inspect`, `--inspect-brk`)
- **WASM plugins** — load and run Rust/C/Go compiled to WebAssembly natively in V8
- **Worker threads** — each worker gets its own V8 isolate, true parallelism (no GIL)
- **Headless server** — no UI in core, any client connects via HTTP/WS

### Server & Networking
- **REST API** — full CRUD for buffers, cursors, state, commands, plugins, help, sessions
- **WebSocket** — real-time bidirectional sync with event streaming
- **Bearer token auth** — secure remote access
- **TLS/SSL** — optional OpenSSL support for encrypted connections
- **Remote editing** — `--remote` flag binds to 0.0.0.0, use with `--token=SECRET`

### Editor Features
- **Multi-buffer tabs** — open, switch, close documents
- **Window splits** — horizontal/vertical splits with binary tree layout
- **Named marks** — buffer-local (a-z) and global (A-Z) marks with jump list
- **Named registers** — yank/delete history, named clipboard (a-z, 0-9, special)
- **Macro recording** — record/playback command sequences
- **Hierarchical keymaps** — mode-specific, buffer-local key bindings with multi-key chords
- **Auto-save** — periodic background save with backup to `~/.berkide/autosave/`
- **Session persistence** — save/restore open files, cursors, layout across restarts
- **Tree-sitter** — syntax parsing engine (optional, compile with `-DBERKIDE_USE_TREESITTER=ON`)
- **Completion engine** — prefix matching with fuzzy scoring
- **Subprocess management** — spawn, pipe I/O, exit tracking for build/lint integration

### By the Numbers
- **28 core C++ subsystems** wired through EditorContext
- **34 JavaScript bindings** on the `editor` global object
- **73 native C++ commands** for cursor, editing, search, undo, marks, macros, folds, windows, sessions, workers
- **100% cross-platform** — macOS, Linux, Windows

---

## Quick Start

### Prerequisites

- C++20 compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.16+
- V8 JavaScript engine (vendored in `third_party/v8/`)
- (Optional) OpenSSL for TLS support

### Build

```bash
# Release build (default)
./build.sh

# Debug build
./build.sh Debug

# With TLS/SSL support
./build.sh --tls

# Manual CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Run

```bash
# Start headless server (localhost only)
./build/berkide

# Remote access with auth
./build/berkide --remote --token=my-secret-token

# Custom ports
./build/berkide --http-port=8080 --ws-port=8081

# TLS (requires --tls build)
./build/berkide --tls-cert=cert.pem --tls-key=key.pem

# Debug JS plugins with Chrome DevTools
./build/berkide --inspect
./build/berkide --inspect-brk              # Break before plugin load
./build/berkide --inspect-port=9230        # Custom port
```

Then open `chrome://inspect` in Chrome to attach the debugger.

### TUI Client

```bash
cd berkide-tui
cmake -B build && cmake --build build -j$(nproc)
./build/berkide-tui              # Connects to ws://127.0.0.1:9002
./build/berkide-tui --url=ws://remote-host:9002?token=SECRET
```

---

## CLI Flags

| Flag | Description |
|------|-------------|
| `--remote` | Bind to 0.0.0.0 (default: 127.0.0.1) |
| `--token=SECRET` | Require Bearer token for HTTP and WS auth |
| `--http-port=PORT` | HTTP REST API port (default: 9001) |
| `--ws-port=PORT` | WebSocket port (default: 9002) |
| `--port=PORT` | Shorthand for --http-port |
| `--tls-cert=FILE` | TLS certificate file (enables TLS) |
| `--tls-key=FILE` | TLS private key file |
| `--tls-ca=FILE` | TLS CA certificate (optional) |
| `--inspect` | Enable V8 Inspector on port 9229 |
| `--inspect-brk` | Enable V8 Inspector, break before plugin load |
| `--inspect-port=PORT` | Custom V8 Inspector port |

---

## JavaScript API Reference

All APIs are available on the global `editor` object in JS plugins.

### editor.buffer
```javascript
editor.buffer.getLine(line)              // Get line content
editor.buffer.lineCount()                // Total line count
editor.buffer.columnCount(line)          // Column count of a line
editor.buffer.insertChar(line, col, ch)  // Insert character
editor.buffer.deleteChar(line, col)      // Delete character
editor.buffer.insertText(line, col, txt) // Insert text
editor.buffer.deleteRange(sl, sc, el, ec)// Delete range
editor.buffer.splitLine(line, col)       // Split line at position
editor.buffer.joinLines(a, b)           // Join two lines
editor.buffer.getText()                  // Get full buffer text
```

### editor.buffers
```javascript
editor.buffers.openFile(path)    // Open file in new tab
editor.buffers.saveActive()      // Save current buffer
editor.buffers.closeActive()     // Close current tab
editor.buffers.next()            // Switch to next tab
editor.buffers.prev()            // Switch to previous tab
editor.buffers.setActive(index)  // Switch to tab by index
editor.buffers.count()           // Number of open buffers
editor.buffers.list()            // List all open files
```

### editor.cursor
```javascript
editor.cursor.getLine()          // Current line
editor.cursor.getCol()           // Current column
editor.cursor.setPosition(l, c)  // Set cursor position
editor.cursor.moveUp()           // Move up
editor.cursor.moveDown()         // Move down
editor.cursor.moveLeft()         // Move left
editor.cursor.moveRight()        // Move right
```

### editor.events
```javascript
editor.events.on("bufferChanged", (path) => { ... })
editor.events.on("cursorMoved", () => { ... })
editor.events.on("modeChanged", (mode) => { ... })
editor.events.on("fileSaved", (path) => { ... })
editor.events.on("tabChanged", () => { ... })
editor.events.emit("customEvent", data)
editor.events.off("eventName", handler)
```

### editor.search
```javascript
editor.search.find(pattern, opts)             // Find next
editor.search.findAll(pattern, opts)          // Find all matches
editor.search.replace(pattern, repl, opts)    // Replace next
editor.search.replaceAll(pattern, repl, opts) // Replace all
// opts: { caseSensitive, regex, wholeWord, wrapAround }
```

### editor.chars
```javascript
editor.chars.classify(char)            // "word"|"whitespace"|"punctuation"|"linebreak"
editor.chars.isWord(char)              // true/false
editor.chars.wordAt(line, col)         // { startCol, endCol, text }
editor.chars.nextWordStart(line, col)  // Next word boundary column
editor.chars.prevWordStart(line, col)  // Previous word boundary column
editor.chars.wordEnd(line, col)        // End of current word
editor.chars.matchBracket(line, col)   // { line, col, bracket } or null
editor.chars.addWordChar(char)         // Add extra word character (e.g. '-' for CSS)
editor.chars.addBracketPair(o, c)      // Add custom bracket pair
```

### editor.indent
```javascript
editor.indent.config()                   // Get current config
editor.indent.config({ useTabs: false, tabWidth: 4, shiftWidth: 4 })
editor.indent.forNewLine(afterLine)      // { level, indentString }
editor.indent.forLine(line)              // { level, indentString }
editor.indent.getLevel(line)             // Indent level number
editor.indent.increase(line)             // Increase indent by 1 level
editor.indent.decrease(line)             // Decrease indent by 1 level
editor.indent.reindent(startLine, endLine) // Reindent range
```

### editor.workers
```javascript
let id = editor.workers.create("worker.js")    // Create from file
let id = editor.workers.createFromSource(code)  // Create from source
editor.workers.postMessage(id, jsonString)      // Send message to worker
editor.workers.terminate(id)                    // Stop worker
editor.workers.terminateAll()                   // Stop all workers
editor.workers.state(id)       // "pending"|"running"|"stopped"|"error"
editor.workers.activeCount()   // Number of running workers
```

Worker script API:
```javascript
// Inside worker.js — each worker has its own V8 isolate
self.onmessage = (event) => {
    let data = JSON.parse(event.data);
    // ... heavy computation (lint, format, compile) ...
    postMessage(JSON.stringify(result));
};
```

### editor.wasm
```javascript
let supported = editor.wasm.isSupported()               // Always true (V8)
let module = editor.wasm.loadFile("plugin.wasm")        // Load .wasm file
let instance = editor.wasm.instantiate(module, imports)  // Create instance
instance.exports.myFunction()                            // Call WASM export
```

### editor.diff
```javascript
editor.diff.diff(textA, textB)               // Line-based diff (Myers)
editor.diff.merge(base, ours, theirs)        // 3-way merge with conflict detection
```

### editor.session
```javascript
editor.session.save()              // Save default session
editor.session.load()              // Load default session
editor.session.saveAs("mywork")    // Save named session
editor.session.loadFrom("mywork")  // Load named session
editor.session.list()              // List saved sessions
editor.session.remove("mywork")    // Delete named session
```

### editor.selection
```javascript
editor.selection.setAnchor(line, col)  // Start selection
editor.selection.isActive()            // Check if selection exists
editor.selection.getRange()            // { startLine, startCol, endLine, endCol }
editor.selection.getText()             // Selected text
editor.selection.clear()               // Clear selection
editor.selection.setType("char"|"line"|"block")
```

### editor.undo
```javascript
editor.undo.undo()           // Undo last change
editor.undo.redo()           // Redo last undo
editor.undo.branchCount()    // Number of undo branches
editor.undo.currentBranch()  // Current branch index
```

### editor.commands
```javascript
// Register a new JS command
editor.commands.register("myPlugin.doSomething", (args) => {
    // ... your logic ...
});

// Execute any command (native or JS)
editor.commands.exec("file.save");
editor.commands.exec("buffer.insert", { text: "hello", line: 0, col: 0 });
```

### Other Bindings
```javascript
// File I/O
editor.file.loadFile(path)       // Read file to string
editor.file.saveFile(path, content) // Write string to file

// Subprocess
editor.process.spawn(cmd, args)  // Run subprocess

// Help system
editor.help.show("topic")       // Show help topic
editor.help.search("query")     // Search help

// Plugins
editor.plugins.list()            // List loaded plugins
editor.plugins.enable(name)      // Enable plugin

// Tree-sitter
editor.treeSitter.parse(lang, source) // Parse with tree-sitter

// Encoding
editor.encoding.detect(data)     // Detect file encoding

// Marks & Registers
editor.marks.set("a", line, col)     // Set named mark
editor.registers.set("a", text)      // Set named register

// Macros
editor.macro.startRecording("q")     // Start recording
editor.macro.stopRecording()         // Stop recording
editor.macro.play("q", count)        // Playback

// Folds
editor.folds.create(startLine, endLine) // Create fold region
editor.folds.toggle(line)               // Toggle fold

// Windows
editor.windows.splitActive("horizontal") // Split window

// Multi-cursor
editor.multiCursor.addCursor(line, col)  // Add cursor

// Keymaps
editor.keymaps.set("normal", "ctrl+s", "file.save") // Bind key

// Extmarks (decorations)
editor.extmarks.set(namespace, line, col, opts)  // Set text decoration

// Completion
editor.completion.complete(prefix)       // Get completions
```

---

## Native Commands (73)

| Category | Commands |
|----------|----------|
| **Input** | `input.key`, `input.char` |
| **Cursor** | `cursor.up`, `cursor.down`, `cursor.left`, `cursor.right`, `cursor.home`, `cursor.end`, `cursor.setPosition` |
| **Buffer** | `buffer.insert`, `buffer.delete`, `buffer.splitLine`, `buffer.new` |
| **Edit** | `edit.undo`, `edit.redo`, `edit.yank`, `edit.paste`, `edit.cut`, `edit.deleteLine` |
| **File** | `file.open`, `file.save`, `file.saveAs` |
| **Tab** | `tab.next`, `tab.prev`, `tab.close`, `tab.switchTo` |
| **Mode** | `mode.set` (normal / insert / visual / visual-line / visual-block) |
| **Selection** | `selection.selectAll` |
| **Search** | `search.forward`, `search.backward`, `search.next`, `search.prev`, `search.replace`, `search.replaceAll` |
| **Mark** | `mark.set`, `mark.jump`, `mark.jumpBack`, `mark.jumpForward` |
| **Fold** | `fold.create`, `fold.toggle`, `fold.collapse`, `fold.expand`, `fold.collapseAll`, `fold.expandAll` |
| **Macro** | `macro.record`, `macro.stop`, `macro.play` |
| **Keymap** | `keymap.set`, `keymap.remove` |
| **Window** | `window.splitH`, `window.splitV`, `window.close`, `window.focusNext`, `window.focusPrev`, `window.equalize` |
| **Multi-cursor** | `multicursor.addAbove`, `multicursor.addBelow`, `multicursor.addNextMatch`, `multicursor.clear` |
| **Session** | `session.save`, `session.saveAs`, `session.load`, `session.delete` |
| **Indent** | `indent.increase`, `indent.decrease`, `indent.reindent` |
| **Worker** | `worker.create`, `worker.terminate`, `worker.terminateAll` |
| **App** | `app.quit`, `app.about` |

---

## REST API

```
GET    /api/state                    # Full editor state (JSON)
GET    /api/buffer                   # Active buffer contents
GET    /api/buffer/line/:n           # Get specific line
POST   /api/buffer/insert            # Insert text { line, col, text }
POST   /api/buffer/delete            # Delete char { line, col }
POST   /api/buffer/edit              # Batch edit operations
POST   /api/buffer/open              # Open file { path }
POST   /api/buffer/save              # Save active buffer
POST   /api/buffer/close             # Close active buffer
GET    /api/buffers                  # List open buffers
POST   /api/buffers/switch           # Switch tab { index }
POST   /api/command                  # Execute command { name, args }
GET    /api/cursor                   # Cursor position
POST   /api/cursor/set               # Set cursor { line, col }
POST   /api/input/key                # Send key event { key }
POST   /api/input/char               # Send character { text }
GET    /api/help                     # List help topics
GET    /api/help/:topic              # Get help topic content
GET    /api/help/search?q=query      # Search help
GET    /api/plugins                  # List plugins
GET    /api/session                  # Current session info
```

### Standard API Response Format

All HTTP endpoints and JS bindings return responses in a unified format:

```json
// Success response
// Basarili yanit
{
  "ok": true,
  "data": { ... },          // Actual payload (any type)
  "meta": { "count": 42 },  // Optional metadata
  "message": "File loaded: /tmp/test.js"
}

// Error response
// Hata yaniti
{
  "ok": false,
  "error": "NOT_FOUND",
  "message": "Buffer not found: scratch"
}
```

| Field     | Type    | Description                                    |
|-----------|---------|------------------------------------------------|
| `ok`      | boolean | `true` = success, `false` = error              |
| `data`    | any     | Response payload (object, array, string, etc.) |
| `meta`    | object  | Optional metadata (counts, pagination, etc.)   |
| `error`   | string  | Error code (only on failure)                   |
| `message` | string  | Human-readable message (localized via i18n)    |

**i18n:** Start the server with `--locale=tr` for Turkish messages. Default is `en`.

```bash
./berkide --locale=tr
```

**JS binding example:**

```javascript
const resp = editor.buffer.getLine(5);
if (resp.ok) {
    console.log(resp.data);      // line content
    console.log(resp.message);   // "Line 5 retrieved" (or Turkish equivalent)
} else {
    console.error(resp.error);   // error code
    console.error(resp.message); // human-readable error
}
```

### WebSocket Protocol

Connect to `ws://localhost:9002` (or `ws://host:port?token=SECRET` for auth).

```json
// Client -> Server: execute command
{ "cmd": "input.char", "args": { "text": "a" } }
{ "cmd": "cursor.up", "args": {} }
{ "cmd": "file.open", "args": { "path": "/tmp/test.js" } }

// Server -> Client: real-time events
{ "type": "fullSync", "data": { "buffer": "...", "cursor": {...} } }
{ "type": "bufferChanged", "data": { "path": "..." } }
{ "type": "cursorMoved", "data": { "line": 5, "col": 12 } }
{ "type": "tabChanged", "data": { ... } }
```

---

## Plugin System — The Emacs Way, Modern Execution

BerkIDE's plugin system is directly inspired by Emacs. JavaScript is the extension language (like Emacs Lisp), and `init.js` is your `init.el`.

### Two Types of Plugins

```
┌──────────────────────────────────────────────────────────────────┐
│ TYPE 1: Live Plugin (like Emacs .el files)                       │
│                                                                   │
│ Write a .js file → save → it works. That's it.                   │
│                                                                   │
│ • ~/.berkide/runtime/init.js  ← Your init.el (loaded first)      │
│ • ~/.berkide/runtime/*.js     ← Instant effect, hot-reload       │
│ • import/export between files                                     │
│ • No build step, no npm, no bundler                               │
│ • Save file → watcher detects → auto-reload                      │
├──────────────────────────────────────────────────────────────────┤
│ TYPE 2: Built Plugin (packaged, publishable)                      │
│                                                                   │
│ Developed separately → npm run build → publish → install          │
│                                                                   │
│ • ~/.berkide/plugins/git-plugin/                                  │
│ •   ├── plugin.json         ← Manifest (name, deps, main)        │
│ •   └── dist/index.js       ← Build output (single bundled file) │
│ • Developed in separate repo with npm (dev dependencies only)     │
│ • npm run build → produces single .js file (all deps bundled)     │
│ • berkide plugin publish → upload to BerkIDE plugin registry      │
│ • berkide plugin install <name> → download and install             │
│ • PluginManager loads with dependency resolution (topo-sort)      │
│ • NO node_modules in plugin folder — only build output            │
└──────────────────────────────────────────────────────────────────┘
```

### init.js — Your Personal Config (like Emacs init.el)

`~/.berkide/runtime/init.js` is always loaded **first** on startup. Use it to configure your editor, import your plugins, and set your preferences.

```javascript
// ~/.berkide/runtime/init.js — Your Emacs init.el equivalent

// Personal settings
editor.options.setDefault('tabWidth', 2);
editor.options.setDefault('useTabs', false);
editor.options.setDefault('scrollOff', 8);
editor.options.setDefault('wordWrap', true);

// Import your live plugins (relative paths)
import './my-keybindings.js';
import './my-theme.js';
import './my-snippets.js';

// Import from .berkide root (@berkide/ prefix)
import '@berkide/plugins/git/index.js';

// Direct configuration
editor.keymaps.set("normal", "ctrl+s", "file.save");

// Event listeners
editor.events.on('bufferOpened', (data) => {
    console.log('File opened:', data.filePath);
});
```

### Boot Order

```
1. ~/.berkide/runtime/init.js   ← YOUR CONFIG (loaded FIRST)
2. ~/.berkide/runtime/*.js      ← Other runtime scripts (alphabetical)
3. ~/.berkide/keymaps/*.js      ← Keymap definitions
4. ~/.berkide/events/*.js       ← Event handlers
5. PluginManager.loadAll()      ← plugins/ folder (dependency-sorted)
```

User files (`~/.berkide/`) override app-bundled files (program folder `.berkide/`).

### Module Resolution

All `.js` files are ES6 modules — `import`/`export` works everywhere.

```javascript
// Relative import (from current file's directory)
import './helpers.js';           // exact file
import './utils';                // tries: utils.js, utils.mjs, utils/index.js

// @berkide/ prefix (resolves to ~/.berkide/)
import '@berkide/plugins/lsp';   // → ~/.berkide/plugins/lsp.js or lsp/index.js

// NOTE: Bare specifiers like 'lodash' do NOT work (no node_modules resolution).
// Built plugins should bundle their dependencies. Live plugins use editor.* API.
```

### Directory Structure
```
~/.berkide/
  runtime/                # init.js + live plugins (loaded first)
    init.js               # Your init.el — personal config, loaded FIRST
    my-keybindings.js     # Live plugin — save and it works
    my-helpers.js         # Shared utilities for your plugins
  keymaps/                # Keymap definition scripts
  events/                 # Event handler scripts
  plugins/                # Installed built plugins
    git-plugin/
      plugin.json         # Plugin manifest
      dist/index.js       # Built output (bundled, no node_modules)
    lsp-client/
      plugin.json
      dist/index.js
  sessions/               # Saved sessions (JSON)
  autosave/               # Auto-save backups
  help/                   # Markdown help topics
  parsers/                # Tree-sitter parser binaries
```

### Plugin Manifest (plugin.json)
```json
{
  "name": "git-plugin",
  "version": "1.0.0",
  "description": "Git integration for BerkIDE",
  "main": "dist/index.js",
  "dependencies": ["process-utils"],
  "berkide": {
    "activationEvents": ["onStartup"]
  }
}
```

### .mjs Support

`.mjs` files are also loaded as ES6 modules (same behavior as `.js`). This is kept for compatibility but **not required** — just use `.js` for everything.

### IIFE Fallback (Legacy)

For rare edge cases where you need classic script isolation (no module scope), the engine provides `loadScriptAsIIFE(path)`. This wraps the file in `(function(){ ... })()` — the old Node.js `require()` era approach. This is **not used by default** and exists only as a fallback. You probably won't need it.

### Hot-Reload

A background file watcher monitors your plugin directories. When you save a `.js` file:

1. Watcher detects the file change
2. Module cache is invalidated for that file
3. File is reloaded as ES6 module
4. Changes take effect immediately — no restart needed

This is the Emacs experience: edit your config, save, see the result.

### Example: Live Plugin — Custom Keybindings
```javascript
// ~/.berkide/runtime/my-keys.js — save this file, it works instantly

editor.keymaps.set("normal", "j", "cursor.down");
editor.keymaps.set("normal", "k", "cursor.up");
editor.keymaps.set("normal", "h", "cursor.left");
editor.keymaps.set("normal", "l", "cursor.right");
editor.keymaps.set("normal", "i", "mode.set", '{"mode":"insert"}');
editor.keymaps.set("insert", "Escape", "mode.set", '{"mode":"normal"}');
editor.keymaps.set("normal", "dd", "edit.deleteLine");
editor.keymaps.set("normal", "/", "search.forward");
```

### Example: Live Plugin — Custom Indent Rules
```javascript
// ~/.berkide/runtime/python-indent.js
export function activate() {
    editor.indent.config({ useTabs: false, shiftWidth: 4 });
    console.log("Python indent rules loaded");
}
```

### Example: Built Plugin — WASM Formatter (Rust/C/Go)
```javascript
// Developed separately, built, published, installed
// This is dist/index.js inside the plugin folder
const mod = editor.wasm.loadFile("./my-formatter.wasm");
const instance = editor.wasm.instantiate(mod, {
    env: { log: (ptr, len) => console.log("WASM:", ptr, len) }
});

export function format(source) {
    return instance.exports.format(source);
}
```

### Example: Background Worker
```javascript
// Spawn a worker for CPU-intensive tasks (linting, formatting)
const workerId = editor.workers.createFromSource(`
    self.onmessage = (event) => {
        const data = JSON.parse(event.data);
        // Heavy computation runs in isolated V8 isolate
        const result = processData(data);
        postMessage(JSON.stringify(result));
    };
`);
editor.workers.postMessage(workerId, JSON.stringify({ file: "main.js" }));
```

### Plugin Development Workflow (Built Plugins)

```
1. Create project:    mkdir my-plugin && cd my-plugin && npm init
2. Develop:           npm install <dev-deps> && write code
3. Build:             npm run build → produces dist/index.js (single file)
4. Test locally:      cp -r dist ~/.berkide/plugins/my-plugin/
5. Publish:           berkide plugin publish
6. Users install:     berkide plugin install my-plugin
```

**Key rule:** `node_modules` is for YOUR dev machine only. The plugin folder contains ONLY the build output. Users never see or need npm.

---

## Default Plugins (Ship with BerkIDE)

BerkIDE comes with 10 default plugins out of the box. All written in JavaScript, all using the `editor.*` API.

| Plugin | Files | What It Does |
|--------|-------|--------------|
| **berkide-default** | 6 | Universal keybindings (Ctrl+S, Ctrl+Z, arrows, Tab, clipboard, tabs) |
| **theme-default** | 5 | Dark + Light color themes, tree-sitter scope mapping, ANSI palette |
| **auto-pairs** | 2 | Auto-close brackets/quotes, smart skip, wrap selection, auto-delete |
| **comment-toggle** | 2 | Toggle line/block comments, 40+ languages, extensible |
| **git** | 5 | Status, diff gutter marks, blame (virtual text), add/commit/push/pull/stash/branch |
| **lsp** | 8 | LSP client (JSON-RPC 2.0) — completion, diagnostics, hover, goto-def, 12 language configs |
| **file-explorer** | 2 | Directory tree logic, gitignore-aware filtering, file CRUD operations |
| **snippet-engine** | 2 | Tab stops, placeholders, built-in snippets for JS/Python/HTML |
| **vim-mode** | 6 | Full vim: Normal/Insert/Visual modes, motions (w/e/b/f/t), operators (d/c/y), text objects |
| **emacs-mode** | 2 | Full emacs: C-x prefix, kill ring, M-x command palette, C-f/C-b/C-n/C-p movement |
| **claude-ai** | 7 | AI assistant — inline completion, code explain/refactor/fix, chat, commit messages. Powered by Claude (Anthropic) |

**Total: 10 plugins, 48 JS files.**

> Vim-mode and emacs-mode are **plugins, not core**. You choose your keybinding style. The editor doesn't force one.

---

## Project Structure

```
berkide/
  src/
    main.cpp                    # Entry point, wiring, CLI parsing
    core/                       # 32 files — Core editor engine
    │  buffer.h/cpp             #   PieceTable-backed text buffer (COW)
    │  PieceTable.h/cpp         #   Line-based piece table implementation
    │  cursor.h/cpp             #   Cursor position and movement
    │  undo.h/cpp               #   Tree-based branching undo/redo
    │  state.h/cpp              #   Per-document state (buf+cur+undo+mode)
    │  buffers.h/cpp            #   Multi-document tab manager
    │  EventBus.h/cpp           #   Thread-safe async pub/sub events
    │  input.h/cpp              #   Keyboard input handler
    │  file.h/cpp               #   File I/O operations
    │  Selection.h/cpp          #   Char/line/block selection
    │  SearchEngine.h/cpp       #   Find/replace with regex
    │  RegisterManager.h/cpp    #   Named registers (yank/paste)
    │  MultiCursor.h/cpp        #   Multiple simultaneous cursors
    │  MacroRecorder.h/cpp      #   Command recording/playback
    │  MarkManager.h/cpp        #   Named marks and jump list
    │  KeymapManager.h/cpp      #   Hierarchical key bindings
    │  FoldManager.h/cpp        #   Code folding regions
    │  WindowManager.h/cpp      #   Split window layout (binary tree)
    │  SessionManager.h/cpp     #   Session persistence (JSON)
    │  AutoSave.h/cpp           #   Background auto-save and backup
    │  ExtmarkManager.h/cpp     #   Text decorations/properties
    │  CharClassifier.h/cpp     #   Character classification, word boundaries
    │  IndentEngine.h/cpp       #   Auto-indent with plugin callback
    │  DiffEngine.h/cpp         #   Myers diff + 3-way merge
    │  CompletionEngine.h/cpp   #   Fuzzy completion scoring
    │  EncodingDetector.h/cpp   #   Encoding detection/conversion
    │  ProcessManager.h/cpp     #   Subprocess lifecycle
    │  WorkerManager.h/cpp      #   Background V8 worker threads
    │  TreeSitterEngine.h/cpp   #   Tree-sitter syntax parsing
    │  PluginManager.h/cpp      #   Plugin discovery and lifecycle
    │  HelpSystem.h/cpp         #   Offline markdown help/wiki
    │  EditorContext.h           #   Central context (28 pointers)
    v8_binding/                 # 38 files — V8 JavaScript bindings
    │  V8Engine.h/cpp           #   V8 lifecycle, script/module loading
    │  InspectorServer.h/cpp    #   Chrome DevTools debugging
    │  BindingRegistry.h/cpp    #   Self-registering binding system
    │  EditorBinding.h/cpp      #   Global 'editor' object creation
    │  BufferBinding.h/cpp      #   editor.buffer.*
    │  CursorBinding.h/cpp      #   editor.cursor.*
    │  BuffersBinding.h/cpp     #   editor.buffers.*
    │  StateBinding.h/cpp       #   editor.state.*
    │  UndoBinding.h/cpp        #   editor.undo.*
    │  EventBinding.h/cpp       #   editor.events.*
    │  InputBinding.h/cpp       #   editor.input.*
    │  FileBinding.h/cpp        #   editor.file.*
    │  SelectionBinding.h/cpp   #   editor.selection.*
    │  SearchBinding.h/cpp      #   editor.search.*
    │  RegisterBinding.h/cpp    #   editor.registers.*
    │  MultiCursorBinding.h/cpp #   editor.multiCursor.*
    │  MacroBinding.h/cpp       #   editor.macro.*
    │  MarkBinding.h/cpp        #   editor.marks.*
    │  KeymapBinding.h/cpp      #   editor.keymaps.*
    │  FoldBinding.h/cpp        #   editor.folds.*
    │  WindowBinding.h/cpp      #   editor.windows.*
    │  SessionBinding.h/cpp     #   editor.session.*
    │  ExtmarkBinding.h/cpp     #   editor.extmarks.*
    │  CharClassifierBinding.h/cpp # editor.chars.*
    │  IndentBinding.h/cpp      #   editor.indent.*
    │  DiffBinding.h/cpp        #   editor.diff.*
    │  CompletionBinding.h/cpp  #   editor.completion.*
    │  EncodingBinding.h/cpp    #   editor.encoding.*
    │  ProcessBinding.h/cpp     #   editor.process.*
    │  WorkerBinding.h/cpp      #   editor.workers.*
    │  WasmBinding.h/cpp        #   editor.wasm.*
    │  TreeSitterBinding.h/cpp  #   editor.treeSitter.*
    │  PluginBinding.h/cpp      #   editor.plugins.*
    │  HelpBinding.h/cpp        #   editor.help.*
    │  HttpServerBinding.h/cpp  #   editor.httpServer.*
    │  WebSocketBinding.h/cpp   #   editor.ws.*
    │  v8_init.h/cpp            #   V8 platform bootstrap
    commands/                   # 6 files — Command system
    │  CommandRegistry.h/cpp    #   Name -> function registry
    │  CommandRouter.h/cpp      #   Dispatcher (C++ then JS fallback)
    │  commands.h/cpp           #   73 native commands
    server/                     # 8 files — Network layer
    │  HttpServer.h/cpp         #   REST API (cpp-httplib)
    │  WebSocketServer.h/cpp    #   Real-time sync (IXWebSocket)
    │  ServerConfig.h           #   Config (ports, auth, TLS)
    │  StateSnapshot.h/cpp      #   State -> JSON serialization
    system/                     # 2 files
    │  Startup.h/cpp            #   Boot and shutdown sequence
    utils/                      # 3 files
       Logger.h                 #   LOG_INFO/ERROR/WARN/DEBUG
       BerkidePaths.h/cpp       #   ~/.berkide/ path resolution
  berkide-tui/                  # Separate TUI client (WebSocket)
  third_party/
    v8/                         # Google V8 JavaScript engine
    ixwebsocket/                # WebSocket library
    http/                       # cpp-httplib (header-only)
    nlohmann/                   # JSON for Modern C++ (header-only)
    tree-sitter/                # Tree-sitter parser library
```

---

## Build Options

| CMake Option | Default | Description |
|---|---|---|
| `CMAKE_BUILD_TYPE` | Release | Build type (Release / Debug / RelWithDebInfo) |
| `BERKIDE_USE_TLS` | OFF | Enable TLS/SSL with OpenSSL |
| `BERKIDE_USE_TREESITTER` | ON | Enable tree-sitter syntax parsing |

---

## Cross-Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| **macOS** (arm64 / x86_64) | Supported | Primary development platform |
| **Linux** (x86_64 / arm64) | Supported | GCC 10+ or Clang 12+ |
| **Windows** (x86_64) | Supported | MSVC 2019+ or MinGW-w64 |

All platform-specific code uses `#ifdef _WIN32` / `#else` guards. I/O uses `std::filesystem`, threading uses `std::thread`/`std::mutex`.

---

## Technology Stack

| Component | Technology |
|-----------|-----------|
| Language | C++20 |
| Script Engine | Google V8 (same engine as Chrome and Node.js) |
| Build System | CMake 3.16+ |
| HTTP Server | cpp-httplib |
| WebSocket | IXWebSocket |
| JSON | nlohmann/json |
| Syntax Parsing | Tree-sitter (optional) |
| TLS/SSL | OpenSSL (optional) |

---

## Code Conventions

- **Bilingual comments**: Line 1 English, Line 2 Turkish on every function
- **Private members**: trailing underscore (`lines_`, `mode_`, `workers_`)
- **Logging**: `LOG_INFO`, `LOG_ERROR`, `LOG_WARN`, `LOG_DEBUG` macros only
- **New subsystem pattern**: C++ class -> EditorContext pointer -> main.cpp wiring -> V8 binding (self-register via BindingRegistry)

---

## Core Development Guide — Adding a New Subsystem

Every core feature in BerkIDE follows the same 7-step pipeline. This ensures all 3 API layers (C++ core, V8 JS, HTTP/WS commands) stay in sync. **No step can be skipped.**

### Step-by-Step Pipeline

```
 Step 1          Step 2           Step 3           Step 4
┌──────────┐   ┌──────────────┐  ┌──────────────┐  ┌───────────────┐
│ C++ Core │──▶│ EditorContext │─▶│ main.cpp     │─▶│ V8 Binding    │
│ Class    │   │ Pointer      │  │ Wiring       │  │ (self-register)│
└──────────┘   └──────────────┘  └──────────────┘  └───────────────┘
                                                           │
 Step 7          Step 6           Step 5                    ▼
┌──────────┐   ┌──────────────┐  ┌──────────────┐  ┌───────────────┐
│ Test &   │◀──│ README &     │◀─│ commands.cpp │◀─│ Binding       │
│ Build    │   │ Docs Update  │  │ Tier 1 API   │  │ Registered    │
└──────────┘   └──────────────┘  └──────────────┘  └───────────────┘
```

### Step 1 — Create Core C++ Class

Location: `src/core/YourFeature.h` + `src/core/YourFeature.cpp`

```cpp
// src/core/YourFeature.h
#pragma once
#include <string>
#include <mutex>

// Short English description of the feature
// Ozelligin kisa Turkce aciklamasi
class YourFeature {
public:
    YourFeature();

    // Public method that does X
    // X yapan genel metod
    void doSomething(const std::string& arg);

    // Query method that returns Y
    // Y donduren sorgu metodu
    std::string getSomething() const;

private:
    mutable std::mutex mutex_;   // Thread safety / Thread guvenligi
    std::string data_;           // Internal state / Dahili durum
};
```

Rules:
- Bilingual comments: English line 1, Turkish line 2. **NO EXCEPTIONS.**
- Private members: trailing underscore (`data_`, `mutex_`)
- Thread safety: use `std::mutex` for shared state
- Cross-platform: `#ifdef _WIN32` / `#else` for OS-specific code
- Use `std::filesystem`, `std::thread`, `std::mutex` (no POSIX-only APIs)

### Step 2 — Add Pointer to EditorContext

Location: `src/core/EditorContext.h`

```cpp
// Add forward declaration at top
class YourFeature;

// Add pointer in struct
struct EditorContext {
    // ... existing 28 pointers ...
    YourFeature* yourFeature = nullptr;  // Your feature / Senin ozelligin
};
```

### Step 3 — Wire in main.cpp

Location: `src/main.cpp`

```cpp
#include "YourFeature.h"

// In main(), after other subsystem initialization:
YourFeature yourFeature;
ctx.yourFeature = &yourFeature;
```

### Step 4 — Create V8 Binding (Tier 2 API)

Location: `src/v8_binding/YourFeatureBinding.h` + `src/v8_binding/YourFeatureBinding.cpp`

Header:
```cpp
// src/v8_binding/YourFeatureBinding.h
#pragma once
#include <v8.h>

struct EditorContext;

// Register editor.yourFeature JS binding
// editor.yourFeature JS binding'ini kaydet
void RegisterYourFeatureBinding(v8::Isolate* isolate,
    v8::Local<v8::Object> editorObj, EditorContext& ctx);
```

Implementation pattern:
```cpp
// src/v8_binding/YourFeatureBinding.cpp
#include "YourFeatureBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "YourFeature.h"
#include <v8.h>

// Register editor.yourFeature JS object with all public methods
// editor.yourFeature JS nesnesini tum genel metodlarla kaydet
void RegisterYourFeatureBinding(v8::Isolate* isolate,
    v8::Local<v8::Object> editorObj, EditorContext& edCtx)
{
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsObj = v8::Object::New(isolate);

    auto* feat = edCtx.yourFeature;

    // yourFeature.doSomething(arg) - Does X
    // X yapar
    jsObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "doSomething"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* f = static_cast<YourFeature*>(
                args.Data().As<v8::External>()->Value());
            if (!f || args.Length() < 1) return;
            v8::String::Utf8Value str(args.GetIsolate(), args[0]);
            f->doSomething(*str ? *str : "");
        }, v8::External::New(isolate, feat)).ToLocalChecked()
    ).Check();

    // yourFeature.getSomething() -> string - Returns Y
    // Y dondurur
    jsObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getSomething"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* f = static_cast<YourFeature*>(
                args.Data().As<v8::External>()->Value());
            if (!f) return;
            auto result = f->getSomething();
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(args.GetIsolate(),
                    result.c_str()).ToLocalChecked());
        }, v8::External::New(isolate, feat)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "yourFeature"),
        jsObj).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _reg = [] {
    BindingRegistry::instance().registerBinding(
        "yourFeature", RegisterYourFeatureBinding);
    return true;
}();
```

Key V8 patterns:
- `v8::External::New(isolate, rawPtr)` to pass C++ pointers to JS callbacks
- `args.Data().As<v8::External>()->Value()` to retrieve the pointer
- `v8::String::Utf8Value` to convert JS strings to C++
- `args.GetReturnValue().Set(...)` to return values to JS
- Static auto-registration via `BindingRegistry::instance().registerBinding()`

### Step 5 — Add Tier 1 Commands (HTTP/WS API)

Location: `src/commands/commands.cpp`

Add both **mutations** (state-changing) and **queries** (read-only):

```cpp
// Include your header at the top
#include "YourFeature.h"

// Inside RegisterCommands():

// --- yourFeature.doSomething: Short description ---
// --- yourFeature.doSomething: Kisa Turkce aciklama ---
router.registerNative("yourFeature.doSomething", [ctx](const json& args) {
    if (!ctx || !ctx->yourFeature) return;
    std::string arg = args.value("arg", "");
    ctx->yourFeature->doSomething(arg);
});

// --- yourFeature.getSomething: Short description ---
// --- yourFeature.getSomething: Kisa Turkce aciklama ---
router.registerQuery("yourFeature.getSomething", [ctx](const json&) -> json {
    if (!ctx || !ctx->yourFeature) return nullptr;
    return ctx->yourFeature->getSomething();
});
```

Rules:
- **Mutations** → `router.registerNative("name", lambda)` — void, changes state
- **Queries** → `router.registerQuery("name", lambda)` — returns `json`, read-only
- Naming: `subsystem.action` format (e.g., `buffer.insert`, `folds.list`)
- Always null-check `ctx` and subsystem pointer
- Commands are automatically available via HTTP (`POST /api/command`) and WebSocket

### Step 6 — Update Documentation

- Update `README.md` command counts and subsystem list
- Update total counts in MEMORY.md
- Add subsystem to EditorContext pointer list

### Step 7 — Build & Verify

```bash
./build.sh          # Must compile with zero errors
# CMakeLists.txt uses GLOB_RECURSE — new .cpp files are auto-discovered
```

---

## Core Development Checklist

Use this checklist when adding ANY new core feature:

```
[ ] 1. Core C++ class created (src/core/Feature.h + .cpp)
    [ ] Bilingual comments (EN line 1, TR line 2) on every function
    [ ] Private members with trailing underscore
    [ ] Thread-safe (std::mutex where needed)
    [ ] Cross-platform (#ifdef _WIN32 where needed)
    [ ] No std::cout (use LOG_INFO/ERROR/WARN/DEBUG)

[ ] 2. EditorContext pointer added (src/core/EditorContext.h)
    [ ] Forward declaration added
    [ ] Pointer initialized to nullptr

[ ] 3. Wired in main.cpp
    [ ] #include added
    [ ] Object created on stack
    [ ] Pointer assigned to ctx

[ ] 4. V8 Binding created (src/v8_binding/FeatureBinding.h + .cpp)
    [ ] All public methods exposed as JS functions
    [ ] Context struct for multi-pointer bindings (if needed)
    [ ] Auto-registered via BindingRegistry static initializer
    [ ] Bilingual comments on every method

[ ] 5. Tier 1 Commands added (src/commands/commands.cpp)
    [ ] All mutations registered via registerNative()
    [ ] All queries registered via registerQuery()
    [ ] Null-checks on ctx and subsystem pointer
    [ ] Bilingual comments on every command
    [ ] Include header added at top of file

[ ] 6. Documentation updated
    [ ] README.md subsystem count updated
    [ ] README.md command count updated (mutations + queries)
    [ ] MEMORY.md EditorContext pointer list updated

[ ] 7. Build passes
    [ ] ./build.sh completes with zero errors
    [ ] No new warnings in project code (V8 warnings are OK)
```

---

## Current API Coverage (Verified)

| Layer | Count | Description |
|-------|-------|-------------|
| **Core C++ Subsystems** | 28 | All wired via EditorContext |
| **Tier 1 — Mutation Commands** | 117 | `registerNative()` in commands.cpp |
| **Tier 1 — Query Commands** | 103 | `registerQuery()` in commands.cpp |
| **Tier 1 — Total** | **220** | HTTP `POST /api/command` + WebSocket |
| **Tier 2 — V8 Binding Files** | 34 | Self-registered via BindingRegistry |
| **Tier 2 — JS Methods** | **333** | Available as `editor.*` in V8 |

### All 28 Core Subsystems

| # | Subsystem | Core Class | V8 Binding | JS Namespace |
|---|-----------|-----------|------------|--------------|
| 1 | Buffer (PieceTable COW) | `Buffer` | `BufferBinding` | `editor.buffer` |
| 2 | Cursor | `Cursor` | `CursorBinding` | `editor.cursor` |
| 3 | Undo (tree + branches) | `UndoManager` | `UndoBinding` | `editor.undo` |
| 4 | Editor State | `EditorState` | `StateBinding` | `editor.state` |
| 5 | Buffers (multi-doc/tabs) | `Buffers` | `BuffersBinding` | `editor.buffers` |
| 6 | Event Bus (async pub/sub) | `EventBus` | `EventBinding` | `editor.events` |
| 7 | Input Handler | `InputHandler` | `InputBinding` | `editor.input` |
| 8 | File System | `FileSystem` | `FileBinding` | `editor.file` |
| 9 | Command Router | `CommandRouter` | `EditorBinding` | `editor.commands` |
| 10 | V8 Engine | `V8Engine` | — | (runtime) |
| 11 | Plugin Manager | `PluginManager` | `PluginBinding` | `editor.plugins` |
| 12 | Help System | `HelpSystem` | `HelpBinding` | `editor.help` |
| 13 | HTTP Server | `HttpServer` | `HttpServerBinding` | `editor.http` |
| 14 | WebSocket Server | `WebSocketServer` | `WebSocketBinding` | `editor.ws` |
| 15 | Process Manager | `ProcessManager` | `ProcessBinding` | `editor.process` |
| 16 | Selection | `Selection` | `SelectionBinding` | `editor.selection` |
| 17 | Register Manager | `RegisterManager` | `RegisterBinding` | `editor.registers` |
| 18 | Search Engine | `SearchEngine` | `SearchBinding` | `editor.search` |
| 19 | Mark Manager | `MarkManager` | `MarkBinding` | `editor.marks` |
| 20 | Auto Save | `AutoSave` | `AutoSaveBinding` | `editor.autosave` |
| 21 | Extmark Manager | `ExtmarkManager` | `ExtmarkBinding` | `editor.extmarks` |
| 22 | Macro Recorder | `MacroRecorder` | `MacroBinding` | `editor.macro` |
| 23 | Keymap Manager | `KeymapManager` | `KeymapBinding` | `editor.keymap` |
| 24 | Fold Manager | `FoldManager` | `FoldBinding` | `editor.folds` |
| 25 | Diff Engine (Myers) | `DiffEngine` | `DiffBinding` | `editor.diff` |
| 26 | Completion Engine | `CompletionEngine` | `CompletionBinding` | `editor.completion` |
| 27 | Multi Cursor | `MultiCursor` | `MultiCursorBinding` | `editor.multicursor` |
| 28 | Window Manager | `WindowManager` | `WindowBinding` | `editor.windows` |
| 29 | Tree-sitter Engine | `TreeSitterEngine` | `TreeSitterBinding` | `editor.treesitter` |
| 30 | Session Manager | `SessionManager` | `SessionBinding` | `editor.session` |
| 31 | Encoding Detector | `EncodingDetector` | `EncodingBinding` | `editor.encoding` |
| 32 | Char Classifier | `CharClassifier` | `CharClassifierBinding` | `editor.chars` |
| 33 | Indent Engine | `IndentEngine` | `IndentBinding` | `editor.indent` |
| 34 | Worker Manager | `WorkerManager` | `WorkerBinding` | `editor.workers` |
| 35 | WASM Support | (V8 native) | `WasmBinding` | `editor.wasm` |
| 36 | V8 Inspector | `InspectorServer` | — | (debug) |

> Note: 28 EditorContext pointers. V8Engine, InspectorServer, WASM, HTTP, WS are infrastructure — not counted as EditorContext subsystems but have full bindings.

---

## License

[GNU Affero General Public License v3.0 (AGPL-3.0)](LICENSE)
