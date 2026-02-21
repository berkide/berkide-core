#pragma once
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include "v8.h"
#include "v8-inspector.h"

// V8 Inspector server for Chrome DevTools debugging of JS plugins.
// JS eklentilerinin Chrome DevTools ile hata ayiklamasi icin V8 Inspector sunucusu.
// Listens on a WebSocket port and bridges DevTools protocol to V8 inspector.
// Bir WebSocket portunda dinler ve DevTools protokolunu V8 inspector'a kopruler.
class InspectorServer : public v8_inspector::V8InspectorClient,
                        public v8_inspector::V8Inspector::Channel {
public:
    InspectorServer();
    ~InspectorServer();

    // Start the inspector server on the given port
    // Inspector sunucusunu verilen portta baslat
    bool start(v8::Isolate* isolate, v8::Local<v8::Context> context, int port = 9229, bool breakOnStart = false);

    // Stop the inspector server
    // Inspector sunucusunu durdur
    void stop();

    // Process pending inspector messages (call from main thread event loop)
    // Bekleyen inspector mesajlarini isle (ana thread olay dongusunden cagir)
    void pumpMessages();

    // Check if inspector is running
    // Inspector'un calisip calismadigini kontrol et
    bool isRunning() const { return running_.load(); }

    // Get the port the inspector is listening on
    // Inspector'un dinledigi portu al
    int port() const { return port_; }

    // V8InspectorClient overrides
    void runMessageLoopOnPause(int contextGroupId) override;
    void quitMessageLoopOnPause() override;

    // V8Inspector::Channel overrides
    void sendResponse(int callId, std::unique_ptr<v8_inspector::StringBuffer> message) override;
    void sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message) override;
    void flushProtocolNotifications() override;

private:
    v8::Isolate* isolate_ = nullptr;
    std::unique_ptr<v8_inspector::V8Inspector> inspector_;
    std::unique_ptr<v8_inspector::V8InspectorSession> session_;
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    int port_ = 9229;

    // WebSocket server thread and message queues
    // WebSocket sunucu thread'i ve mesaj kuyruklari
    std::thread wsThread_;
    std::mutex inMutex_;
    std::queue<std::string> inQueue_;     // DevTools -> V8
    std::mutex outMutex_;
    std::queue<std::string> outQueue_;    // V8 -> DevTools
    std::atomic<bool> clientConnected_{false};

    // Convert V8 inspector StringView to std::string
    // V8 inspector StringView'u std::string'e donustur
    static std::string stringViewToUtf8(const v8_inspector::StringView& view);

    // WebSocket server loop
    // WebSocket sunucu dongusu
    void wsServerLoop();
};
