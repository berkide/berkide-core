// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <queue>
#include <condition_variable>

// Message passed between main thread and worker threads
// Ana thread ve calisan thread'ler arasinda iletilen mesaj
struct WorkerMessage {
    std::string data;     // Message payload (JSON string) / Mesaj yukü (JSON dizesi)
    int workerId = -1;    // Source worker ID (-1 = main) / Kaynak calisan ID'si (-1 = ana)
};

// Worker state
// Calisan durumu
enum class WorkerState { Pending, Running, Stopped, Error };

// Callback for messages from workers
// Calisanlardan gelen mesajlar icin geri cagirim
using WorkerMessageCallback = std::function<void(int workerId, const std::string& message)>;

// Manages background V8 isolate workers for CPU-intensive plugin tasks.
// CPU yogun eklenti gorevleri icin arka plan V8 izolasyonu calisanlarini yonetir.
// Each worker runs in its own thread with its own V8 isolate (no GIL, true parallelism).
// Her calisan kendi V8 izolasyonuyla kendi thread'inde calisir (GIL yok, gercek paralellik).
class WorkerManager {
public:
    WorkerManager();
    ~WorkerManager();

    // Create a new worker that runs a script file, returns worker ID
    // Bir betik dosyasi calistiran yeni calisan olustur, calisan ID'si dondur
    int createWorker(const std::string& scriptPath);

    // Create a worker from inline script source
    // Satir ici betik kaynagindan calisan olustur
    int createWorkerFromSource(const std::string& source);

    // Post a message to a worker
    // Bir calisana mesaj gonder
    bool postMessage(int workerId, const std::string& message);

    // Terminate a specific worker
    // Belirli bir calisani sonlandir
    bool terminate(int workerId);

    // Terminate all workers
    // Tum calisanlari sonlandir
    void terminateAll();

    // Get state of a worker
    // Bir calisanin durumunu al
    WorkerState getState(int workerId) const;

    // Get number of active workers
    // Aktif calisan sayisini al
    int activeCount() const;

    // Set callback for messages from workers to main thread
    // Calisanlardan ana thread'e mesajlar icin geri cagirim ayarla
    void setMessageCallback(WorkerMessageCallback cb);

    // Process pending messages from workers (call from main thread)
    // Calisanlardan bekleyen mesajlari isle (ana thread'den cagir)
    void processPendingMessages();

private:
    struct Worker {
        int id;
        std::string scriptPath;
        std::string scriptSource;
        std::thread thread;
        std::atomic<WorkerState> state{WorkerState::Pending};
        std::atomic<bool> shouldStop{false};

        // Inbound message queue (main -> worker)
        // Gelen mesaj kuyrugu (ana -> calisan)
        std::mutex inMutex;
        std::condition_variable inCv;
        std::queue<std::string> inQueue;
    };

    mutable std::mutex mutex_;
    std::unordered_map<int, std::unique_ptr<Worker>> workers_;
    std::atomic<int> nextId_{1};

    // Outbound message queue (worker -> main)
    // Giden mesaj kuyrugu (calisan -> ana)
    std::mutex outMutex_;
    std::queue<WorkerMessage> outQueue_;
    WorkerMessageCallback messageCallback_;

    // Worker thread entry point
    // Calisan thread giris noktasi
    void workerThreadFunc(Worker* worker);

    // Post message from worker to main thread
    // Calisandan ana thread'e mesaj gonder
    void postToMain(int workerId, const std::string& message);
};
