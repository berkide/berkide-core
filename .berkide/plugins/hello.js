// BerkIDE welcome plugin — startup banner and system info
// BerkIDE karsilama eklentisi — baslatma afisi ve sistem bilgisi
(function() {
    const RST  = "\x1b[0m";
    const BOLD = "\x1b[1m";
    const DIM  = "\x1b[2m";
    const WHITE = "\x1b[97m";
    const GRAY  = "\x1b[90m";
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
        GRAY + "  The Editor Engine That Gets Out of Your Way" + RST,
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
        const commands = editor.commands.list();
        const totalCmds = commands ? commands.length : 0;
        const bufferCount = editor.buffers.count();
        const bindings = Object.getOwnPropertyNames(editor).filter(
            k => typeof editor[k] === "object" && editor[k] !== null
        );

        console.log("  " + BRAND + "\u25CF" + RST + " " + WHITE + "Commands" + RST + "    " + GRAY + totalCmds + " registered" + RST);
        console.log("  " + BRAND + "\u25CF" + RST + " " + WHITE + "Bindings" + RST + "    " + GRAY + bindings.length + " namespaces on editor.*" + RST);
        console.log("  " + BRAND + "\u25CF" + RST + " " + WHITE + "Buffers" + RST + "     " + GRAY + bufferCount + " open" + RST);
        console.log("");
    } catch(e) {
        console.warn("  Stats unavailable: " + e.message);
    }

    console.log("  " + GRAY + "HTTP  " + RST + DIM + "http://127.0.0.1:1881" + RST);
    console.log("  " + GRAY + "WS    " + RST + DIM + "ws://127.0.0.1:1882" + RST);
    console.log("");
})();
