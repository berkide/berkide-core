#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <future>

// Describes a granular buffer change for incremental processing.
// Artimsal isleme icin ayrintili buffer degisikligini tanimlar.
// Emitted with "bufferChanged" events so plugins can efficiently update.
// Plugin'lerin verimli guncelleme yapabilmesi icin "bufferChanged" olaylariyla yayinlanir.
struct ChangeEvent {
    int startLine = 0;      // First changed line / Ilk degisen satir
    int startCol  = 0;      // First changed column / Ilk degisen sutun
    int oldEndLine = 0;     // End line before change / Degisiklikten onceki bitis satiri
    int oldEndCol  = 0;     // End column before change / Degisiklikten onceki bitis sutunu
    int newEndLine = 0;     // End line after change / Degisiklikten sonraki bitis satiri
    int newEndCol  = 0;     // End column after change / Degisiklikten sonraki bitis sutunu
    int linesAdded = 0;     // Net lines added (negative = deleted) / Net eklenen satirlar (negatif = silinen)
    std::string text;       // Text that was inserted (empty for delete) / Eklenen metin (silme icin bos)
    std::string filePath;   // Buffer file path / Buffer dosya yolu
};

// Serialize a ChangeEvent to JSON string for EventBus payload
// ChangeEvent'i EventBus yuku icin JSON dizesine serile et
inline std::string changeEventToJson(const ChangeEvent& ce) {
    // Manual JSON to avoid nlohmann dependency in this header
    // Bu header'da nlohmann bagimliligini onlemek icin manuel JSON
    return "{\"startLine\":" + std::to_string(ce.startLine) +
           ",\"startCol\":" + std::to_string(ce.startCol) +
           ",\"oldEndLine\":" + std::to_string(ce.oldEndLine) +
           ",\"oldEndCol\":" + std::to_string(ce.oldEndCol) +
           ",\"newEndLine\":" + std::to_string(ce.newEndLine) +
           ",\"newEndCol\":" + std::to_string(ce.newEndCol) +
           ",\"linesAdded\":" + std::to_string(ce.linesAdded) +
           ",\"filePath\":\"" + ce.filePath + "\"}";
}

// Thread-safe asynchronous event system with priority and wildcard support.
// Oncelik ve joker karakter destekli, thread-safe asenkron olay sistemi.
// Used as the central pub/sub backbone for all editor components.
// Tum editor bilesenleri icin merkezi yayinla/abone ol omurgasi olarak kullanilir.
class EventBus {
public:
    // Represents a single event with a name and JSON payload
    // Bir isim ve JSON yukuyle tek bir olayi temsil eder
    struct Event {
        std::string name;
        std::string payload;
    };

    // Callback type for event listeners
    // Olay dinleyicileri icin geri cagirim turu
    using Listener = std::function<void(const Event&)>;

    // Internal handler struct with priority and one-shot flag
    // Oncelik ve tek seferlik bayrak iceren dahili isleyici yapisi
    struct Handler {
        Listener callback;
        int priority;       // Higher priority runs first / Yuksek oncelik once calisir
        bool once;          // If true, removed after first call / True ise ilk cagirdan sonra kaldirilir
    };

    EventBus();
    ~EventBus();

    // Subscribe to an event (persistent listener)
    // Bir olaya abone ol (kalici dinleyici)
    void on(const std::string& event, Listener fn, int priority = 0);

    // Subscribe for a single occurrence only
    // Yalnizca tek bir olusum icin abone ol
    void once(const std::string& event, Listener fn, int priority = 0);

    // Emit an event asynchronously (queued for dispatch thread)
    // Bir olayi asenkron olarak yayinla (dagitim thread'i icin kuyruga alinir)
    void emit(const std::string& event, const std::string& payload = "{}");

    // Emit an event synchronously (blocks until all handlers complete)
    // Bir olayi senkron olarak yayinla (tum isleyiciler tamamlanana kadar bloklar)
    void emitSync(const std::string& event, const std::string& payload = "{}");

    // Remove all listeners for a specific event
    // Belirli bir olay icin tum dinleyicileri kaldir
    void off(const std::string& event);

    // Gracefully shutdown the dispatch thread and clear all state
    // Dagitim thread'ini duzgun kapat ve tum durumu temizle
    void shutdown();

private:
    // Background dispatch loop that processes the event queue
    // Olay kuyruguhu isleyen arka plan dagitim dongusu
    void dispatchLoop();

    std::unordered_map<std::string, std::vector<Handler>> listeners_;  // Event -> handlers map / Olay -> isleyiciler haritasi
    std::mutex mutex_;                                                  // Protects listeners_ / listeners_'i korur

    std::queue<Event> queue_;              // Async event queue / Asenkron olay kuyrugu
    std::mutex queueMutex_;                // Protects queue_ / queue_'yu korur
    std::condition_variable cv_;           // Wakes dispatch thread / Dagitim thread'ini uyandirr

    std::atomic<bool> running_{true};      // Controls dispatch loop lifecycle / Dagitim dongusu yasam dongusunu kontrol eder
    std::thread worker_;                   // Background dispatch thread / Arka plan dagitim thread'i
};
