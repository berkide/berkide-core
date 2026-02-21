#pragma once
#include <iostream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdlib>

// Log severity levels from lowest (Debug) to highest (Error)
// En dusukten (Debug) en yuksege (Error) log ciddiyet seviyeleri
enum class LogLevel { Debug, Info, Warn, Error };

// Thread-safe singleton logger with modern terminal output.
// Modern terminal ciktili, thread-safe tekil loglayici.
//
// Output format:
//   20:33:14.818  INFO  [Startup] Engine initialized.
//   ^^^^^^^^^^^^  ^^^^  ^^^^^^^^^ ^^^^^^^^^^^^^^^^^^^^
//   dim gray      level tag color  white
//
// Each [Tag] gets its own color based on module name.
// Her [Tag] modul adina gore kendi rengini alir.
//
// Respects NO_COLOR environment variable.
// NO_COLOR ortam degiskenine saygi gosterir.
class Logger {
public:
    // Singleton access
    // Tekil erisim
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    // Set minimum log level (messages below this level are suppressed)
    // Minimum log seviyesini ayarla (bu seviyenin altindaki mesajlar bastrilir)
    void setLevel(LogLevel level) { level_ = level; }

    // Get current log level
    // Mevcut log seviyesini al
    LogLevel getLevel() const { return level_; }

    // Enable or disable colored output (default: auto-detect)
    // Renkli ciktiyi etkinlestir veya devre disi birak (varsayilan: otomatik)
    void setColor(bool enabled) { color_ = enabled; }

    // Log at Debug level
    // Debug seviyesinde logla
    template<typename... Args>
    void debug(Args&&... args) { log(LogLevel::Debug, std::forward<Args>(args)...); }

    // Log at Info level
    // Info seviyesinde logla
    template<typename... Args>
    void info(Args&&... args) { log(LogLevel::Info, std::forward<Args>(args)...); }

    // Log at Warn level (goes to stderr)
    // Warn seviyesinde logla (stderr'e gider)
    template<typename... Args>
    void warn(Args&&... args) { log(LogLevel::Warn, std::forward<Args>(args)...); }

    // Log at Error level (goes to stderr)
    // Error seviyesinde logla (stderr'e gider)
    template<typename... Args>
    void error(Args&&... args) { log(LogLevel::Error, std::forward<Args>(args)...); }

private:
    Logger() {
        // Respect NO_COLOR convention (https://no-color.org)
        // NO_COLOR kuralina saygi goster
        const char* nc = std::getenv("NO_COLOR");
        if (nc && nc[0] != '\0') color_ = false;
    }

    LogLevel level_ = LogLevel::Info;
    bool color_ = true;
    std::mutex mutex_;

    // ANSI escape codes
    // ANSI kacis kodlari
    static constexpr const char* RST  = "\x1b[0m";
    static constexpr const char* BOLD = "\x1b[1m";
    static constexpr const char* DIM  = "\x1b[2m";

    // Foreground colors
    // On plan renkleri
    static constexpr const char* FG_WHITE   = "\x1b[97m";
    static constexpr const char* FG_GRAY    = "\x1b[90m";
    static constexpr const char* FG_RED     = "\x1b[31m";
    static constexpr const char* FG_GREEN   = "\x1b[32m";
    static constexpr const char* FG_YELLOW  = "\x1b[33m";
    static constexpr const char* FG_BLUE    = "\x1b[34m";
    static constexpr const char* FG_MAGENTA = "\x1b[35m";
    static constexpr const char* FG_CYAN    = "\x1b[36m";

    // Extended 256-color codes for more variety
    // Daha fazla cesitlilik icin 256-renk kodlari
    static constexpr const char* FG_ORANGE  = "\x1b[38;5;208m";
    static constexpr const char* FG_PINK    = "\x1b[38;5;205m";
    static constexpr const char* FG_TEAL    = "\x1b[38;5;43m";
    static constexpr const char* FG_LIME    = "\x1b[38;5;154m";
    static constexpr const char* FG_PURPLE  = "\x1b[38;5;141m";
    static constexpr const char* FG_SKY     = "\x1b[38;5;117m";

    // Get current time as HH:MM:SS.mmm string
    // Mevcut zamani HH:MM:SS.mmm dizesi olarak al
    static std::string timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif
        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(2) << tm.tm_hour << ":"
            << std::setw(2) << tm.tm_min << ":"
            << std::setw(2) << tm.tm_sec << "."
            << std::setw(3) << ms.count();
        return oss.str();
    }

    // Level badge color and text
    // Seviye rozeti rengi ve metni
    static constexpr const char* levelColor(LogLevel level) {
        switch (level) {
            case LogLevel::Debug: return FG_GRAY;
            case LogLevel::Info:  return FG_CYAN;
            case LogLevel::Warn:  return FG_YELLOW;
            case LogLevel::Error: return FG_RED;
        }
        return "";
    }

    static constexpr const char* levelText(LogLevel level) {
        switch (level) {
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info:  return " INFO";
            case LogLevel::Warn:  return " WARN";
            case LogLevel::Error: return "ERROR";
        }
        return "";
    }

    // Get color for a known module tag, or hash-based color for unknown tags
    // Bilinen modul etiketi icin renk al, bilinmeyenler icin hash tabanli renk
    static const char* tagColor(const std::string& tag) {
        // Known module colors
        // Bilinen modul renkleri
        if (tag == "Startup" || tag == "Berkide" || tag == "berkide")
                                    return FG_GREEN;
        if (tag == "V8")            return FG_YELLOW;
        if (tag == "HTTP")          return FG_CYAN;
        if (tag == "WS")            return FG_TEAL;
        if (tag == "Command")       return FG_MAGENTA;
        if (tag == "Plugin")        return FG_BLUE;
        if (tag == "Keymap")        return FG_ORANGE;
        if (tag == "Events")        return FG_PURPLE;
        if (tag == "Process")       return FG_PINK;
        if (tag == "Worker")        return FG_LIME;
        if (tag == "Search")        return FG_SKY;
        if (tag == "AutoSave")      return FG_GRAY;
        if (tag == "Session")       return FG_TEAL;
        if (tag == "Help")          return FG_GRAY;
        if (tag == "Core")          return FG_GREEN;

        // Unknown tags — hash the name to pick a color
        // Bilinmeyen etiketler — ismi hashleyerek renk sec
        static const char* palette[] = {
            FG_CYAN, FG_GREEN, FG_YELLOW, FG_BLUE,
            FG_MAGENTA, FG_ORANGE, FG_PINK, FG_TEAL,
            FG_LIME, FG_PURPLE, FG_SKY
        };
        unsigned hash = 0;
        for (char c : tag) hash = hash * 31 + static_cast<unsigned>(c);
        return palette[hash % 11];
    }

    // Colorize all [Tag] parts in a message with per-module colors
    // Mesajdaki tum [Tag] kisimlarini modul bazli renklerle renklendir
    static std::string colorizeTags(const std::string& msg) {
        std::string result;
        result.reserve(msg.size() + 128);
        size_t i = 0;
        while (i < msg.size()) {
            if (msg[i] == '[') {
                size_t end = msg.find(']', i);
                if (end != std::string::npos && end - i < 30 && end > i + 1) {
                    std::string tag = msg.substr(i + 1, end - i - 1);
                    const char* color = tagColor(tag);
                    result += color;
                    result += BOLD;
                    result += '[';
                    result += tag;
                    result += ']';
                    result += RST;
                    result += FG_WHITE;
                    i = end + 1;
                    continue;
                }
            }
            result += msg[i];
            ++i;
        }
        return result;
    }

    // Flatten variadic args into a single string
    // Degisken sayida argumani tek bir dizeye duz yaz
    template<typename... Args>
    static std::string flatten(Args&&... args) {
        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));
        return oss.str();
    }

    // Core log function — modern colored output
    // Temel log fonksiyonu — modern renkli cikti
    template<typename... Args>
    void log(LogLevel level, Args&&... args) {
        if (level < level_) return;

        std::string msg = flatten(std::forward<Args>(args)...);

        std::lock_guard<std::mutex> lock(mutex_);
        auto& out = (level >= LogLevel::Warn) ? std::cerr : std::cout;

        if (color_) {
            out << DIM << FG_GRAY << timestamp() << RST
                << "  "
                << levelColor(level) << BOLD << levelText(level) << RST
                << "  "
                << FG_WHITE << colorizeTags(msg) << RST
                << "\n";
        } else {
            out << timestamp() << "  " << levelText(level) << "  " << msg << "\n";
        }
    }
};

// Convenience macros for quick logging access
// Hizli loglama erisimi icin kolaylik makrolari
#define LOG_DEBUG(...) Logger::instance().debug(__VA_ARGS__)
#define LOG_INFO(...)  Logger::instance().info(__VA_ARGS__)
#define LOG_WARN(...)  Logger::instance().warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::instance().error(__VA_ARGS__)
