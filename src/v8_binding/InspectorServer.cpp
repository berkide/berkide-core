// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "InspectorServer.h"
#include "Logger.h"
#include <ixwebsocket/IXWebSocketServer.h>
#include <cstring>

// Constructor
// Kurucu
InspectorServer::InspectorServer() {}

// Destructor: ensure clean shutdown
// Yikici: temiz kapatmayi garanti et
InspectorServer::~InspectorServer() {
    stop();
}

// Convert V8 inspector StringView to UTF-8 string
// V8 inspector StringView'u UTF-8 dizesine donustur
std::string InspectorServer::stringViewToUtf8(const v8_inspector::StringView& view) {
    if (view.is8Bit()) {
        return std::string(reinterpret_cast<const char*>(view.characters8()), view.length());
    }
    // Convert UTF-16 to UTF-8
    // UTF-16'yi UTF-8'e donustur
    std::string result;
    result.reserve(view.length() * 3);
    for (size_t i = 0; i < view.length(); ++i) {
        uint16_t ch = view.characters16()[i];
        if (ch < 0x80) {
            result.push_back(static_cast<char>(ch));
        } else if (ch < 0x800) {
            result.push_back(static_cast<char>(0xC0 | (ch >> 6)));
            result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xE0 | (ch >> 12)));
            result.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        }
    }
    return result;
}

// Start the inspector server: create V8Inspector, start WebSocket listener
// Inspector sunucusunu baslat: V8Inspector olustur, WebSocket dinleyicisini baslat
bool InspectorServer::start(v8::Isolate* isolate, v8::Local<v8::Context> context,
                            int port, bool breakOnStart) {
    if (running_.load()) return false;
    isolate_ = isolate;
    port_ = port;

    // Create V8 inspector instance
    // V8 inspector ornegi olustur
    inspector_ = v8_inspector::V8Inspector::create(isolate, this);

    // Register the context with the inspector
    // Baglami inspector ile kaydet
    v8_inspector::StringView contextName(
        reinterpret_cast<const uint8_t*>("BerkIDE"), 7);
    inspector_->contextCreated(v8_inspector::V8ContextInfo(context, 1, contextName));

    // Create a session connected to this channel
    // Bu kanala bagli bir oturum olustur
    v8_inspector::StringView emptyState;
    session_ = inspector_->connect(1, this, emptyState,
        v8_inspector::V8Inspector::ClientTrustLevel::kFullyTrusted);

    running_.store(true);

    // Start WebSocket server in background thread
    // Arka plan thread'inde WebSocket sunucusunu baslat
    wsThread_ = std::thread(&InspectorServer::wsServerLoop, this);

    LOG_INFO("[Inspector] Listening on ws://127.0.0.1:", port);
    LOG_INFO("[Inspector] Open chrome://inspect in Chrome to debug");

    // If --inspect-brk, schedule a debugger break
    // Eger --inspect-brk ise, bir hata ayiklayici duraksatmasi zamanla
    if (breakOnStart) {
        v8_inspector::StringView reason(
            reinterpret_cast<const uint8_t*>("Break on start"), 14);
        session_->schedulePauseOnNextStatement(reason, reason);
        LOG_INFO("[Inspector] Waiting for debugger to connect (--inspect-brk)...");
    }

    return true;
}

// Stop the inspector server
// Inspector sunucusunu durdur
void InspectorServer::stop() {
    if (!running_.load()) return;
    running_.store(false);
    paused_.store(false);

    if (wsThread_.joinable()) {
        wsThread_.join();
    }

    session_.reset();
    inspector_.reset();
    isolate_ = nullptr;

    LOG_INFO("[Inspector] Server stopped");
}

// Process pending messages from DevTools to V8
// DevTools'tan V8'e bekleyen mesajlari isle
void InspectorServer::pumpMessages() {
    if (!session_ || !running_.load()) return;

    std::queue<std::string> msgs;
    {
        std::lock_guard<std::mutex> lock(inMutex_);
        std::swap(msgs, inQueue_);
    }

    while (!msgs.empty()) {
        const std::string& msg = msgs.front();
        v8_inspector::StringView messageView(
            reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
        session_->dispatchProtocolMessage(messageView);
        msgs.pop();
    }
}

// Called by V8 when execution is paused (breakpoint hit)
// V8 tarafindan yurutme duraksatildiginda cagrilir (kesme noktasi isabet)
void InspectorServer::runMessageLoopOnPause(int /*contextGroupId*/) {
    paused_.store(true);
    // Spin and process DevTools messages while paused
    // Duraksatilmisken devam et ve DevTools mesajlarini isle
    while (paused_.load() && running_.load()) {
        pumpMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Called by V8 when execution resumes from pause
// V8 tarafindan yurutme duraksatmadan devam ettiginde cagrilir
void InspectorServer::quitMessageLoopOnPause() {
    paused_.store(false);
}

// Send response from V8 inspector to DevTools
// V8 inspector'dan DevTools'a yanit gonder
void InspectorServer::sendResponse(int /*callId*/,
                                    std::unique_ptr<v8_inspector::StringBuffer> message) {
    std::string msg = stringViewToUtf8(message->string());
    std::lock_guard<std::mutex> lock(outMutex_);
    outQueue_.push(std::move(msg));
}

// Send notification from V8 inspector to DevTools
// V8 inspector'dan DevTools'a bildirim gonder
void InspectorServer::sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message) {
    std::string msg = stringViewToUtf8(message->string());
    std::lock_guard<std::mutex> lock(outMutex_);
    outQueue_.push(std::move(msg));
}

// Flush pending protocol notifications
// Bekleyen protokol bildirimlerini temizle
void InspectorServer::flushProtocolNotifications() {
    // No buffering needed, messages are sent immediately
    // Tamponlama gerekmez, mesajlar hemen gonderilir
}

// WebSocket server loop: accept connections and relay messages
// WebSocket sunucu dongusu: baglantilari kabul et ve mesajlari aktar
void InspectorServer::wsServerLoop() {
    ix::WebSocketServer server(port_, "127.0.0.1");

    server.setOnClientMessageCallback(
        [this](std::shared_ptr<ix::ConnectionState> /*connState*/,
               ix::WebSocket& ws,
               const ix::WebSocketMessagePtr& msg) {

            if (msg->type == ix::WebSocketMessageType::Open) {
                clientConnected_.store(true);
                LOG_INFO("[Inspector] DevTools client connected");
            }
            else if (msg->type == ix::WebSocketMessageType::Close) {
                clientConnected_.store(false);
                LOG_INFO("[Inspector] DevTools client disconnected");
            }
            else if (msg->type == ix::WebSocketMessageType::Message) {
                // Forward DevTools message to V8 inspector
                // DevTools mesajini V8 inspector'a ilet
                {
                    std::lock_guard<std::mutex> lock(inMutex_);
                    inQueue_.push(msg->str);
                }

                // Send any pending outbound messages back to DevTools
                // Bekleyen giden mesajlari DevTools'a geri gonder
                std::queue<std::string> outMsgs;
                {
                    std::lock_guard<std::mutex> lock(outMutex_);
                    std::swap(outMsgs, outQueue_);
                }
                while (!outMsgs.empty()) {
                    ws.send(outMsgs.front());
                    outMsgs.pop();
                }
            }
        });

    auto res = server.listen();
    if (!res.first) {
        LOG_ERROR("[Inspector] Failed to listen on port ", port_, ": ", res.second);
        running_.store(false);
        return;
    }

    server.start();

    // Keep server alive while running, also flush outgoing messages
    // Calisirken sunucuyu canli tut, ayrica giden mesajlari temizle
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Periodically flush outbound messages to connected clients
        // Periyodik olarak giden mesajlari bagli istemcilere temizle
        if (clientConnected_.load()) {
            std::queue<std::string> outMsgs;
            {
                std::lock_guard<std::mutex> lock(outMutex_);
                std::swap(outMsgs, outQueue_);
            }
            // Note: IXWebSocket server doesn't directly expose per-client send
            // The messages will be sent on next client message via the callback
            // Not: IXWebSocket sunucusu istemci bazinda dogrudan send sunmaz
            // Mesajlar sonraki istemci mesajinda callback araciligiyla gonderilecek
            if (!outMsgs.empty()) {
                std::lock_guard<std::mutex> lock(outMutex_);
                while (!outMsgs.empty()) {
                    outQueue_.push(std::move(outMsgs.front()));
                    outMsgs.pop();
                }
            }
        }
    }

    server.stop();
}
