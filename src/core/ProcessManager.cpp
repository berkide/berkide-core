// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "ProcessManager.h"
#include "EventBus.h"
#include "Logger.h"
#include <cstring>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/wait.h>
    #include <sys/select.h>
    #include <fcntl.h>
    #include <csignal>
    #include <cerrno>
#endif

// Constructor: nothing to initialize beyond defaults
// Kurucu: varsayilanlar disinda baslatilacak bir sey yok
ProcessManager::ProcessManager() = default;

// Destructor: shut down all remaining processes
// Yikici: kalan tum surecleri kapat
ProcessManager::~ProcessManager() {
    shutdownAll();
}

#ifndef _WIN32

// Set a file descriptor to non-blocking mode
// Bir dosya tanimlayicisini engellemesiz moda ayarla
static void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

// Spawn a new child process using fork+exec with piped stdin/stdout/stderr
// fork+exec kullanarak pipe'li stdin/stdout/stderr ile yeni bir alt surec baslat
int ProcessManager::spawn(const std::string& command, const std::vector<std::string>& args,
                          const ProcessOptions& opts) {
    // Create pipes: [0]=read, [1]=write
    // Pipe'lar olustur: [0]=okuma, [1]=yazma
    int stdinPipe[2], stdoutPipe[2], stderrPipe[2];

    if (pipe(stdinPipe) != 0 || pipe(stdoutPipe) != 0) {
        LOG_ERROR("[Process] Failed to create pipes: ", strerror(errno));
        return -1;
    }

    bool hasStderr = !opts.mergeStderr;
    if (hasStderr && pipe(stderrPipe) != 0) {
        LOG_ERROR("[Process] Failed to create stderr pipe: ", strerror(errno));
        close(stdinPipe[0]); close(stdinPipe[1]);
        close(stdoutPipe[0]); close(stdoutPipe[1]);
        return -1;
    }

    berk_pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("[Process] Fork failed: ", strerror(errno));
        close(stdinPipe[0]); close(stdinPipe[1]);
        close(stdoutPipe[0]); close(stdoutPipe[1]);
        if (hasStderr) { close(stderrPipe[0]); close(stderrPipe[1]); }
        return -1;
    }

    if (pid == 0) {
        // Child process: wire pipes to stdin/stdout/stderr
        // Alt surec: pipe'lari stdin/stdout/stderr'a bagla
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        if (hasStderr) close(stderrPipe[0]);

        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);

        if (hasStderr) {
            dup2(stderrPipe[1], STDERR_FILENO);
        } else {
            dup2(stdoutPipe[1], STDERR_FILENO);
        }

        close(stdinPipe[0]);
        close(stdoutPipe[1]);
        if (hasStderr) close(stderrPipe[1]);

        // Change working directory if specified
        // Belirtilmisse calisma dizinini degistir
        if (!opts.cwd.empty()) {
            if (chdir(opts.cwd.c_str()) != 0) {
                _exit(127);
            }
        }

        // Set extra environment variables
        // Ekstra ortam degiskenlerini ayarla
        for (const auto& envVar : opts.env) {
            putenv(const_cast<char*>(envVar.c_str()));
        }

        // Build argv array for execvp
        // execvp icin argv dizisi olustur
        std::vector<const char*> argv;
        argv.push_back(command.c_str());
        for (const auto& a : args) {
            argv.push_back(a.c_str());
        }
        argv.push_back(nullptr);

        execvp(command.c_str(), const_cast<char* const*>(argv.data()));

        // If execvp returns, it failed
        // execvp donerse basarisiz olmustur
        _exit(127);
    }

    // Parent process: close child's pipe ends, keep ours
    // Ana surec: alt surecin pipe uclarini kapat, bizimkileri tut
    close(stdinPipe[0]);
    close(stdoutPipe[1]);
    if (hasStderr) close(stderrPipe[1]);

    setNonBlocking(stdoutPipe[0]);
    if (hasStderr) setNonBlocking(stderrPipe[0]);

    int id = nextId_.fetch_add(1);

    auto entry = std::make_unique<ProcessEntry>();
    entry->handle.id = id;
    entry->handle.pid = pid;
    entry->handle.running = true;
    entry->stdinFd  = stdinPipe[1];
    entry->stdoutFd = stdoutPipe[0];
    entry->stderrFd = hasStderr ? stderrPipe[0] : -1;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        processes_[id] = std::move(entry);
    }

    // Start background reader thread for this process
    // Bu surec icin arka plan okuyucu thread'i baslat
    {
        std::lock_guard<std::mutex> lock(mutex_);
        processes_[id]->readerThread = std::thread(&ProcessManager::readerLoop, this, id);
    }

    LOG_INFO("[Process] Spawned: ", command, " (id=", id, ", pid=", pid, ")");

    if (eventBus_) {
        eventBus_->emit("processStarted", "{\"id\":" + std::to_string(id) +
                         ",\"pid\":" + std::to_string(pid) +
                         ",\"command\":\"" + command + "\"}");
    }

    return id;
}

#else // _WIN32

int ProcessManager::spawn(const std::string& command, const std::vector<std::string>& args,
                          const ProcessOptions& opts) {
    // Windows implementation using CreateProcess
    // CreateProcess kullanarak Windows implementasyonu
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE stdinRead, stdinWrite, stdoutRead, stdoutWrite, stderrRead, stderrWrite;

    if (!CreatePipe(&stdinRead, &stdinWrite, &sa, 0) ||
        !CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0)) {
        LOG_ERROR("[Process] Failed to create pipes");
        return -1;
    }

    bool hasStderr = !opts.mergeStderr;
    if (hasStderr && !CreatePipe(&stderrRead, &stderrWrite, &sa, 0)) {
        CloseHandle(stdinRead); CloseHandle(stdinWrite);
        CloseHandle(stdoutRead); CloseHandle(stdoutWrite);
        return -1;
    }

    // Prevent child from inheriting our pipe ends
    // Alt surecin bizim pipe uclarimizi miras almasini engelle
    SetHandleInformation(stdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);
    if (hasStderr) SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = stdinRead;
    si.hStdOutput = stdoutWrite;
    si.hStdError  = hasStderr ? stderrWrite : stdoutWrite;

    // Build command line string
    // Komut satiri dizesi olustur
    std::string cmdLine = command;
    for (const auto& a : args) {
        cmdLine += " " + a;
    }

    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessA(
        nullptr, const_cast<char*>(cmdLine.c_str()),
        nullptr, nullptr, TRUE, 0, nullptr,
        opts.cwd.empty() ? nullptr : opts.cwd.c_str(),
        &si, &pi);

    CloseHandle(stdinRead);
    CloseHandle(stdoutWrite);
    if (hasStderr) CloseHandle(stderrWrite);

    if (!ok) {
        LOG_ERROR("[Process] CreateProcess failed: ", GetLastError());
        CloseHandle(stdinWrite);
        CloseHandle(stdoutRead);
        if (hasStderr) CloseHandle(stderrRead);
        return -1;
    }

    CloseHandle(pi.hThread);

    int id = nextId_.fetch_add(1);

    auto entry = std::make_unique<ProcessEntry>();
    entry->handle.id = id;
    entry->handle.pid = static_cast<berk_pid_t>(pi.dwProcessId);
    entry->handle.running = true;
    // Store Windows HANDLEs as fd ints (cast for uniform API)
    entry->stdinFd  = reinterpret_cast<intptr_t>(stdinWrite);
    entry->stdoutFd = reinterpret_cast<intptr_t>(stdoutRead);
    entry->stderrFd = hasStderr ? reinterpret_cast<intptr_t>(stderrRead) : -1;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        processes_[id] = std::move(entry);
        processes_[id]->readerThread = std::thread(&ProcessManager::readerLoop, this, id);
    }

    LOG_INFO("[Process] Spawned: ", command, " (id=", id, ", pid=", pi.dwProcessId, ")");
    return id;
}

#endif // _WIN32

// Write data to a process's stdin pipe
// Bir surecin stdin pipe'ina veri yaz
bool ProcessManager::write(int processId, const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(processId);
    if (it == processes_.end() || it->second->stdinFd < 0) return false;

#ifdef _WIN32
    DWORD written;
    HANDLE h = reinterpret_cast<HANDLE>(static_cast<intptr_t>(it->second->stdinFd));
    return WriteFile(h, data.c_str(), static_cast<DWORD>(data.size()), &written, nullptr) != 0;
#else
    ssize_t n = ::write(it->second->stdinFd, data.c_str(), data.size());
    return n >= 0;
#endif
}

// Close the stdin pipe (sends EOF to child process)
// Stdin pipe'ini kapat (alt surece EOF gonderir)
bool ProcessManager::closeStdin(int processId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(processId);
    if (it == processes_.end() || it->second->stdinFd < 0) return false;

#ifdef _WIN32
    CloseHandle(reinterpret_cast<HANDLE>(static_cast<intptr_t>(it->second->stdinFd)));
#else
    close(it->second->stdinFd);
#endif
    it->second->stdinFd = -1;
    return true;
}

// Send a signal to a process
// Bir surece sinyal gonder
bool ProcessManager::signal(int processId, int sig) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(processId);
    if (it == processes_.end() || !it->second->handle.running) return false;

#ifdef _WIN32
    (void)sig;
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, it->second->handle.pid);
    if (!h) return false;
    bool ok = TerminateProcess(h, 1) != 0;
    CloseHandle(h);
    return ok;
#else
    return ::kill(it->second->handle.pid, sig) == 0;
#endif
}

// Kill a process forcefully (SIGKILL / TerminateProcess)
// Bir sureci zorla oldur (SIGKILL / TerminateProcess)
bool ProcessManager::kill(int processId) {
#ifdef _WIN32
    return signal(processId, 0);
#else
    return signal(processId, SIGKILL);
#endif
}

// Check if a process is still running
// Bir surecin hala calismakta olup olmadigini kontrol et
bool ProcessManager::isRunning(int processId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(processId);
    if (it == processes_.end()) return false;
    return it->second->handle.running;
}

// Get process info by ID
// Kimlige gore surec bilgisi al
const ProcessHandle* ProcessManager::getProcess(int processId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(processId);
    if (it == processes_.end()) return nullptr;
    return &it->second->handle;
}

// List all tracked processes
// Takip edilen tum surecleri listele
std::vector<ProcessHandle> ProcessManager::list() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ProcessHandle> result;
    result.reserve(processes_.size());
    for (auto& [id, entry] : processes_) {
        result.push_back(entry->handle);
    }
    return result;
}

// Set stdout callback for a process
// Bir surec icin stdout geri cagirimini ayarla
void ProcessManager::onStdout(int processId, ProcessOutputCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(processId);
    if (it != processes_.end()) it->second->onStdout = std::move(cb);
}

// Set stderr callback for a process
// Bir surec icin stderr geri cagirimini ayarla
void ProcessManager::onStderr(int processId, ProcessOutputCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(processId);
    if (it != processes_.end()) it->second->onStderr = std::move(cb);
}

// Set exit callback for a process
// Bir surec icin cikis geri cagirimini ayarla
void ProcessManager::onExit(int processId, ProcessExitCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(processId);
    if (it != processes_.end()) it->second->onExit = std::move(cb);
}

// Background reader loop: reads stdout/stderr and invokes callbacks
// Arka plan okuyucu dongusu: stdout/stderr okur ve geri cagirimlari cagir
void ProcessManager::readerLoop(int processId) {
    char buf[4096];

    while (true) {
        int stdoutFd = -1, stderrFd = -1;
        bool running = false;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = processes_.find(processId);
            if (it == processes_.end()) return;
            stdoutFd = it->second->stdoutFd;
            stderrFd = it->second->stderrFd;
            running = it->second->handle.running;
        }

        if (stdoutFd < 0 && stderrFd < 0) break;

#ifdef _WIN32
        // Windows: read stdout in blocking mode
        // Windows: stdout'u engellemeli modda oku
        if (stdoutFd >= 0) {
            HANDLE h = reinterpret_cast<HANDLE>(static_cast<intptr_t>(stdoutFd));
            DWORD bytesRead = 0;
            if (ReadFile(h, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0) {
                std::string data(buf, bytesRead);
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = processes_.find(processId);
                if (it != processes_.end() && it->second->onStdout) {
                    it->second->onStdout(processId, data);
                }
            } else {
                break;
            }
        }
#else
        // POSIX: use select() to multiplex stdout and stderr
        // POSIX: stdout ve stderr'i coklayiciya select() ile bagla
        fd_set readFds;
        FD_ZERO(&readFds);
        int maxFd = -1;

        if (stdoutFd >= 0) { FD_SET(stdoutFd, &readFds); maxFd = std::max(maxFd, stdoutFd); }
        if (stderrFd >= 0) { FD_SET(stderrFd, &readFds); maxFd = std::max(maxFd, stderrFd); }

        if (maxFd < 0) break;

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 50000; // 50ms poll interval / 50ms yoklama araligi

        int ret = select(maxFd + 1, &readFds, nullptr, nullptr, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (ret == 0) {
            // Timeout: check if process is still running
            // Zaman asimi: surecin hala calisip calismadigini kontrol et
            if (!running) {
                // Drain remaining data after process exit
                // Surec cikisindan sonra kalan veriyi bosalt
                while (stdoutFd >= 0) {
                    ssize_t n = read(stdoutFd, buf, sizeof(buf));
                    if (n <= 0) break;
                    std::string data(buf, static_cast<size_t>(n));
                    std::lock_guard<std::mutex> lock(mutex_);
                    auto it = processes_.find(processId);
                    if (it != processes_.end() && it->second->onStdout) {
                        it->second->onStdout(processId, data);
                    }
                }
                break;
            }
            continue;
        }

        bool anyRead = false;

        if (stdoutFd >= 0 && FD_ISSET(stdoutFd, &readFds)) {
            ssize_t n = read(stdoutFd, buf, sizeof(buf));
            if (n > 0) {
                anyRead = true;
                std::string data(buf, static_cast<size_t>(n));
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = processes_.find(processId);
                if (it != processes_.end() && it->second->onStdout) {
                    it->second->onStdout(processId, data);
                }
                if (eventBus_) {
                    eventBus_->emit("processStdout", "{\"id\":" + std::to_string(processId) + "}");
                }
            } else if (n == 0) {
                // EOF on stdout
                // stdout'ta dosya sonu
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = processes_.find(processId);
                if (it != processes_.end()) {
                    close(it->second->stdoutFd);
                    it->second->stdoutFd = -1;
                }
            }
        }

        if (stderrFd >= 0 && FD_ISSET(stderrFd, &readFds)) {
            ssize_t n = read(stderrFd, buf, sizeof(buf));
            if (n > 0) {
                anyRead = true;
                std::string data(buf, static_cast<size_t>(n));
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = processes_.find(processId);
                if (it != processes_.end() && it->second->onStderr) {
                    it->second->onStderr(processId, data);
                }
                if (eventBus_) {
                    eventBus_->emit("processStderr", "{\"id\":" + std::to_string(processId) + "}");
                }
            } else if (n == 0) {
                // EOF on stderr
                // stderr'da dosya sonu
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = processes_.find(processId);
                if (it != processes_.end()) {
                    close(it->second->stderrFd);
                    it->second->stderrFd = -1;
                }
            }
        }

        // If both pipes closed and process not running, exit reader
        // Her iki pipe da kapaliysa ve surec calismiyorsa, okuyucudan cik
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = processes_.find(processId);
            if (it != processes_.end()) {
                if (it->second->stdoutFd < 0 && it->second->stderrFd < 0) {
                    break;
                }
            }
        }
#endif
    }

    // Wait for process exit and collect exit code
    // Surec cikisini bekle ve cikis kodunu topla
    waitForExit(processId);
}

// Wait for process to finish and collect exit code
// Surecin bitmesini bekle ve cikis kodunu topla
void ProcessManager::waitForExit(int processId) {
    berk_pid_t pid;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = processes_.find(processId);
        if (it == processes_.end()) return;
        pid = it->second->handle.pid;
    }

#ifdef _WIN32
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (h) {
        WaitForSingleObject(h, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(h, &code);
        CloseHandle(h);

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = processes_.find(processId);
        if (it != processes_.end()) {
            it->second->handle.running = false;
            it->second->handle.exitCode = static_cast<int>(code);
            if (it->second->onExit) it->second->onExit(processId, static_cast<int>(code));
        }
    }
#else
    int status = 0;
    berk_pid_t result = waitpid(pid, &status, 0);
    int exitCode = -1;

    if (result > 0) {
        if (WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            exitCode = 128 + WTERMSIG(status);
        }
    }

    ProcessExitCallback exitCb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = processes_.find(processId);
        if (it != processes_.end()) {
            it->second->handle.running = false;
            it->second->handle.exitCode = exitCode;
            closeFds(*it->second);
            exitCb = it->second->onExit;
        }
    }

    if (exitCb) exitCb(processId, exitCode);

    if (eventBus_) {
        eventBus_->emit("processExit", "{\"id\":" + std::to_string(processId) +
                         ",\"exitCode\":" + std::to_string(exitCode) + "}");
    }

    LOG_INFO("[Process] Exited: id=", processId, " code=", exitCode);
#endif
}

// Close all open file descriptors for a process
// Bir surec icin tum acik dosya tanimlayicilarini kapat
void ProcessManager::closeFds(ProcessEntry& entry) {
#ifdef _WIN32
    auto closeH = [](int fd) {
        if (fd >= 0) CloseHandle(reinterpret_cast<HANDLE>(static_cast<intptr_t>(fd)));
    };
    closeH(entry.stdinFd);
    closeH(entry.stdoutFd);
    closeH(entry.stderrFd);
#else
    if (entry.stdinFd >= 0)  close(entry.stdinFd);
    if (entry.stdoutFd >= 0) close(entry.stdoutFd);
    if (entry.stderrFd >= 0) close(entry.stderrFd);
#endif
    entry.stdinFd = entry.stdoutFd = entry.stderrFd = -1;
}

// Shut down all processes: kill running ones, join reader threads
// Tum surecleri kapat: calisanlari oldur, okuyucu thread'lerini birlesir
void ProcessManager::shutdownAll() {
    std::vector<int> ids;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, entry] : processes_) {
            ids.push_back(id);
        }
    }

    // Kill all running processes
    // Calisan tum surecleri oldur
    for (int id : ids) {
        if (isRunning(id)) {
#ifdef _WIN32
            kill(id);
#else
            signal(id, SIGTERM);
#endif
        }
    }

    // Join all reader threads
    // Tum okuyucu thread'lerini birlestir
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, entry] : processes_) {
            if (entry->readerThread.joinable()) {
                entry->readerThread.detach();
            }
            closeFds(*entry);
        }
        processes_.clear();
    }

    LOG_INFO("[Process] All processes shut down");
}
