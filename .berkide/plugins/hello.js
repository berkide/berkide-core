// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

// BerkIDE welcome plugin — startup banner and system info
// BerkIDE karsilama eklentisi — baslatma afisi ve sistem bilgisi
(function () {
    const RST = "\x1b[0m";
    const BOLD = "\x1b[1m";
    const DIM = "\x1b[2m";
    const WHITE = "\x1b[97m";
    const GRAY = "\x1b[90m";
    const BRAND = "\x1b[38;2;255;140;0m"; // #FF8C00 — BerkIDE Brand Orange

    const VERSION = editor.version || "unknown";

    // ASCII art banner in brand color (ANSI Shadow font)
    // Marka renginde ASCII sanat afisi (ANSI Shadow fontu)
    const banner = [
        "",
        BRAND + BOLD +
        " ██████╗ ███████╗██████╗ ██╗  ██╗██╗██████╗ ███████╗" + RST,
        BRAND + BOLD +
        " ██╔══██╗██╔════╝██╔══██╗██║ ██╔╝██║██╔══██╗██╔════╝" + RST,
        BRAND + BOLD +
        " ██████╔╝█████╗  ██████╔╝█████╔╝ ██║██║  ██║█████╗  " + RST,
        BRAND + BOLD +
        " ██╔══██╗██╔══╝  ██╔══██╗██╔═██╗ ██║██║  ██║██╔══╝  " + RST,
        BRAND + BOLD +
        " ██████╔╝███████╗██║  ██║██║  ██╗██║██████╔╝███████╗" + RST,
        BRAND + BOLD +
        " ╚═════╝ ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝╚═╝╚═════╝ ╚══════╝" + RST,
        "",
        GRAY + "  No impositions." + RST,
        "",
    ];

    banner.forEach(line => console.log(line));

    // Version + tech stack line (right after banner)
    // Versiyon + teknoloji satiri (bannerdan hemen sonra)
    console.log("  " + BRAND + BOLD + "v" + VERSION + RST + "  " + GRAY + "|" + RST + "  " + WHITE + "C++ core" + RST + "  " + GRAY + "|" + RST + "  " + WHITE + "V8 JavaScript" + RST + "  " + GRAY + "|" + RST + "  " + WHITE + "Headless server" + RST);
    console.log("");

    // Collect system info (wrapped in try/catch to not break startup)
    // Sistem bilgilerini topla (baslatmayi bozmasin diye try/catch icinde)
    try {
        // editor.commands.list() returns plain array (from bindCommandsAPI, not ApiResponse)
        // editor.commands.list() duz dizi dondurur (bindCommandsAPI'den, ApiResponse degil)
        const cmdList = editor.commands.list();
        const totalCmds = Array.isArray(cmdList) ? cmdList.length : 0;

        // editor.buffers may not exist — check safely
        // editor.buffers mevcut olmayabilir — guvenli kontrol et
        let bufferCount = 0;
        if (editor.buffers && typeof editor.buffers.count === "function") {
            const countResp = editor.buffers.count();
            bufferCount = (countResp && countResp.ok) ? countResp.data : (typeof countResp === "number" ? countResp : 0);
        }

        const bindings = Object.getOwnPropertyNames(editor).filter(
            k => typeof editor[k] === "object" && editor[k] !== null
        );
        const cppCount = editor.__sources ? Object.keys(editor.__sources.cpp || {}).length : 0;
        const jsCount = editor.__sources ? Object.keys(editor.__sources.js || {}).length : 0;

        console.log("  " + BRAND + "\u25CF" + RST + " " + WHITE + "Commands" + RST + "    " + GRAY + totalCmds + " registered" + RST);
        console.log("  " + BRAND + "\u25CF" + RST + " " + WHITE + "Bindings" + RST + "    " + GRAY + (cppCount + jsCount) + " namespaces on editor.*" + RST);
        console.log("                    " + GRAY + "* C++ " + cppCount + RST);
        console.log("                    " + GRAY + "* JS  " + jsCount + RST);
        console.log("  " + BRAND + "\u25CF" + RST + " " + WHITE + "Buffers" + RST + "     " + GRAY + bufferCount + " open" + RST);
        console.log("");
    } catch (e) {
        console.warn("  Stats unavailable: " + e.message);
    }

    const httpPort = editor.config.get("server.http_port") || 1881;
    const wsPort = editor.config.get("server.ws_port") || 1882;
    const bindAddr = editor.config.get("server.bind_address") || "127.0.0.1";
    console.log("  " + GRAY + "HTTP  " + RST + DIM + "http://" + bindAddr + ":" + httpPort + RST);
    console.log("  " + GRAY + "WS    " + RST + DIM + "ws://" + bindAddr + ":" + wsPort + RST);
    console.log("");
    console.log("  " + DIM + "Free software by Berk Co\u015Far \u2014 AGPL v3" + RST);
    console.log("  " + DIM + "https://github.com/berkide/berkide-core" + RST);
    console.log("");
})();
