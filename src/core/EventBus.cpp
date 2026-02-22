// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "EventBus.h"
#include "Logger.h"
#include <algorithm>

// Constructor: start the background dispatch thread
// Kurucu: arka plan dagitim is parcacigini baslat
EventBus::EventBus() {
    worker_ = std::thread(&EventBus::dispatchLoop, this);
}

// Destructor: shut down the event bus gracefully
// Yikici: olay veri yolunu duzgunce kapat
EventBus::~EventBus() {
    shutdown();
}

// Register a persistent listener for a named event with priority ordering
// Adlandirilmis bir olay icin oncelik sirasiyla kalici bir dinleyici kaydet
void EventBus::on(const std::string& event, Listener fn, int priority) {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_[event].push_back({std::move(fn), priority, false});
    std::sort(listeners_[event].begin(), listeners_[event].end(),
        [](const Handler& a, const Handler& b){ return a.priority > b.priority; });
}

// Register a one-time listener that is removed after its first invocation
// Ilk cagrisinin ardindan kaldirilan tek seferlik bir dinleyici kaydet
void EventBus::once(const std::string& event, Listener fn, int priority) {
    std::lock_guard<std::mutex> lock(mutex_);
    listeners_[event].push_back({std::move(fn), priority, true});
    std::sort(listeners_[event].begin(), listeners_[event].end(),
        [](const Handler& a, const Handler& b){ return a.priority > b.priority; });
}

// Emit an event asynchronously by adding it to the dispatch queue
// Dagitim kuyraguna ekleyerek bir olayi asenkron olarak yayinla
void EventBus::emit(const std::string& event, const std::string& payload) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queue_.push({event, payload});
    }
    cv_.notify_one();
}

// Emit an event synchronously: invoke all matching listeners on the calling thread
// Bir olayi senkron olarak yayinla: cagiran is parcaciginda tum eslesen dinleyicileri calistir
void EventBus::emitSync(const std::string& event, const std::string& payload) {
    std::vector<Handler> targets;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        // normal event listener'larını al
        if (auto it = listeners_.find(event); it != listeners_.end())
            targets.insert(targets.end(), it->second.begin(), it->second.end());

        // wildcard listener'ları da ekle
        if (auto it = listeners_.find("*"); it != listeners_.end())
            targets.insert(targets.end(), it->second.begin(), it->second.end());
    }

    // çağır
    for (auto&& h : std::move(targets)) {
    try {
        h.callback({event, payload});
        } catch (const std::exception& e) {
            LOG_ERROR("[EventBus] Error in handler (", event, "): ", e.what());
        } catch (...) {
            LOG_ERROR("[EventBus] Unknown error in handler (", event, ")");
        }
    }

    // once handler'ları güvenli kaldır
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto clean = [](std::vector<Handler>& vec) {
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                [](const Handler& h){ return h.once; }), vec.end());
        };
        if (listeners_.count(event)) clean(listeners_[event]);
        if (listeners_.count("*")) clean(listeners_["*"]);
    }
}

// Remove all listeners registered for a specific event
// Belirli bir olay icin kayitli tum dinleyicileri kaldir
void EventBus::off(const std::string& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = listeners_.find(event);
    if (it != listeners_.end()) {
        listeners_.erase(it);
    }
}

// Background worker loop: wait for events in queue and dispatch them
// Arka plan calisan dongusu: kuyrukta olay bekle ve onlari dagit
void EventBus::dispatchLoop() {
    while (running_) {
        Event e;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            cv_.wait(lock, [&]{ return !queue_.empty() || !running_; });
            if (!running_) break;
            e = queue_.front();
            queue_.pop();
        }
        emitSync(e.name, e.payload);
    }
}

// Shut down the event bus: stop worker thread, clear queue and listeners
// Olay veri yolunu kapat: calisan is parcacigini durdur, kuyruk ve dinleyicileri temizle
void EventBus::shutdown() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false))
        return; // zaten kapalıysa çık

    cv_.notify_all();
    if (worker_.joinable()) worker_.join();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        std::queue<Event>().swap(queue_); // kuyruk temizle
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        listeners_.clear(); // tüm handler’ları kaldır
    }

    LOG_INFO("[EventBus] Shutdown complete");
}