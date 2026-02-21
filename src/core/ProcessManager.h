#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>

// Cross-platform process ID type
// Platformlar arasi surec kimlik turu
#ifdef _WIN32
    using berk_pid_t = unsigned long;    // DWORD on Windows / Windows'ta DWORD
#else
    #include <sys/types.h>
    using berk_pid_t = pid_t;            // pid_t on POSIX / POSIX'te pid_t
#endif

// Configuration options for spawning a child process
// Alt surec baslatma icin yapilandirma secenekleri
struct ProcessOptions {
    std::string cwd;                     // Working directory (empty = inherit) / Calisma dizini (bos = miras al)
    std::vector<std::string> env;        // Extra env vars "KEY=VALUE" / Ekstra ortam degiskenleri "KEY=DEGER"
    bool mergeStderr = false;            // Redirect stderr to stdout / Stderr'i stdout'a yonlendir
};

// Represents a running or finished child process
// Calisan veya tamamlanmis bir alt sureci temsil eder
struct ProcessHandle {
    int id = 0;                          // Internal process ID / Dahili surec kimigi
    berk_pid_t pid = 0;                  // OS process ID / Isletim sistemi surec kimigi
    bool running = false;                // Whether the process is still alive / Surecin hala calip olup olmadigi
    int exitCode = -1;                   // Exit code after termination / Sonlanmadan sonraki cikis kodu
};

// Callback types for process I/O events
// Surec giris/cikis olaylari icin geri cagirim turleri
using ProcessOutputCallback = std::function<void(int processId, const std::string& data)>;
using ProcessExitCallback   = std::function<void(int processId, int exitCode)>;

class EventBus;

// Manages child process lifecycle: spawn, pipe I/O, signal, kill.
// Alt surec yasam dongusunu yonetir: baslatma, pipe giris/cikis, sinyal, durdurma.
// This is the foundation for LSP, Git, linters, formatters, and any external tool integration.
// LSP, Git, linter, formatter ve herhangi bir dis arac entegrasyonunun temelidir.
// Cross-platform: POSIX (macOS/Linux) via fork+exec, Windows via CreateProcess.
// Platformlar arasi: POSIX (macOS/Linux) fork+exec ile, Windows CreateProcess ile.
class ProcessManager {
public:
    ProcessManager();
    ~ProcessManager();

    // Set event bus for emitting process events
    // Surec olaylarini yayinlamak icin olay veri yolunu ayarla
    void setEventBus(EventBus* eb) { eventBus_ = eb; }

    // Spawn a new child process with piped stdin/stdout/stderr
    // Pipe'li stdin/stdout/stderr ile yeni bir alt surec baslat
    // Returns process ID (internal, not OS PID) or -1 on failure
    // Surec kimligini dondurur (dahili, OS PID degil) veya basarisizlikta -1
    int spawn(const std::string& command, const std::vector<std::string>& args,
              const ProcessOptions& opts = {});

    // Write data to a process's stdin pipe
    // Bir surecin stdin pipe'ina veri yaz
    bool write(int processId, const std::string& data);

    // Close the stdin pipe of a process (signals EOF to the child)
    // Bir surecin stdin pipe'ini kapat (alt surece EOF sinyali gonderir)
    bool closeStdin(int processId);

    // Send a signal to a process (SIGTERM, SIGKILL, etc.)
    // Bir surece sinyal gonder (SIGTERM, SIGKILL, vb.)
    bool signal(int processId, int sig);

    // Kill a process (SIGKILL on POSIX, TerminateProcess on Windows)
    // Bir sureci oldur (POSIX'te SIGKILL, Windows'ta TerminateProcess)
    bool kill(int processId);

    // Check if a process is still running
    // Bir surecin hala calismakta olup olmadigini kontrol et
    bool isRunning(int processId) const;

    // Get process info by ID
    // Kimlige gore surec bilgisi al
    const ProcessHandle* getProcess(int processId) const;

    // List all tracked processes
    // Takip edilen tum surecleri listele
    std::vector<ProcessHandle> list() const;

    // Set callback for stdout data from a specific process
    // Belirli bir surecten stdout verisi icin geri cagirim ayarla
    void onStdout(int processId, ProcessOutputCallback cb);

    // Set callback for stderr data from a specific process
    // Belirli bir surecten stderr verisi icin geri cagirim ayarla
    void onStderr(int processId, ProcessOutputCallback cb);

    // Set callback for process exit
    // Surec cikisi icin geri cagirim ayarla
    void onExit(int processId, ProcessExitCallback cb);

    // Shut down all processes and reader threads
    // Tum surecleri ve okuyucu thread'leri kapat
    void shutdownAll();

private:
    // Internal state for a managed process
    // Yonetilen bir surec icin dahili durum
    struct ProcessEntry {
        ProcessHandle handle;
        int stdinFd  = -1;               // Pipe write end for child's stdin / Alt surecin stdin'i icin pipe yazma ucu
        int stdoutFd = -1;               // Pipe read end for child's stdout / Alt surecin stdout'u icin pipe okuma ucu
        int stderrFd = -1;               // Pipe read end for child's stderr / Alt surecin stderr'i icin pipe okuma ucu
        std::thread readerThread;         // Background thread reading stdout/stderr / Stdout/stderr okuyan arka plan thread'i
        ProcessOutputCallback onStdout;   // stdout callback / stdout geri cagrimi
        ProcessOutputCallback onStderr;   // stderr callback / stderr geri cagrimi
        ProcessExitCallback onExit;       // exit callback / cikis geri cagrimi
    };

    // Background reader that polls stdout/stderr and invokes callbacks
    // Stdout/stderr'i yoklayan ve geri cagirimlari cagiran arka plan okuyucu
    void readerLoop(int processId);

    // Wait for a process to exit and collect its exit code
    // Bir surecin cikmajsini bekle ve cikis kodunu topla
    void waitForExit(int processId);

    // Close all file descriptors for a process entry
    // Bir surec girisi icin tum dosya tanimlayicilarini kapat
    void closeFds(ProcessEntry& entry);

    mutable std::mutex mutex_;
    std::unordered_map<int, std::unique_ptr<ProcessEntry>> processes_;
    std::atomic<int> nextId_{1};
    EventBus* eventBus_ = nullptr;
};
