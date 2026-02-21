#pragma once
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXWebSocketMessageType.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <set>
#include <string>
#include "nlohmann/json.hpp"
#include "ServerConfig.h"
#include "EditorContext.h"

using json = nlohmann::json;

// WebSocket server for real-time editor state synchronization.
// Gercek zamanli editor durumu senkronizasyonu icin WebSocket sunucusu.
// Broadcasts state changes to all connected clients (cursor moves, buffer edits, tab switches).
// Durum degisikliklerini tum bagli istemcilere yayinlar (imlec hareketleri, buffer duzenlemeleri, sekme degisiklikleri).
class WebSocketServer {
public:
    WebSocketServer();
    ~WebSocketServer();

    // Set the editor context for accessing real editor objects
    // Gercek editor nesnelerine erismek icin editor baglamini ayarla
    void setEditorContext(EditorContext* ctx) { edCtx_ = ctx; }

    // Start the server on a specific port (uses default config)
    // Sunucuyu belirli bir portta baslat (varsayilan yapilandirmayi kullanir)
    void start(int port = 1882);

    // Start the server with full configuration
    // Sunucuyu tam yapilandirmayla baslat
    void start(const ServerConfig& config);

    // Stop the server and disconnect all clients
    // Sunucuyu durdur ve tum istemcilerin baglantilarini kes
    void stop();

    // Send a message to all connected clients
    // Tum bagli istemcilere mesaj gonder
    void broadcast(const std::string& msg);

    // Broadcast a structured event with JSON data to all clients
    // Tum istemcilere JSON verili yapilandirilmis bir olay yayinla
    void broadcastEvent(const std::string& event, const json& data);

    // Check if the server is currently running
    // Sunucunun su anda calisip calismadigini kontrol et
    bool isRunning() const { return running_; }

private:
    // Validate authentication token from WebSocket URL query params
    // WebSocket URL sorgu parametrelerinden kimlik dogrulama token'ini dogrula
    bool validateToken(const std::string& url) const;

    // Register EventBus listeners for real-time sync broadcasting
    // Gercek zamanli senkronizasyon yayini icin EventBus dinleyicilerini kaydet
    void setupEventBusListeners();

    std::atomic<bool> running_{false};                   // Server running state / Sunucu calisma durumu
    std::unique_ptr<ix::WebSocketServer> server_;         // IXWebSocket server instance / IXWebSocket sunucu ornegi
    std::set<ix::WebSocket*> clients_;                    // Connected clients / Bagli istemciler
    std::mutex clientsMutex_;                              // Protects clients_ set / clients_ setini korur
    ServerConfig config_;                                  // Server configuration / Sunucu yapilandirmasi
    EditorContext* edCtx_ = nullptr;                       // Editor context / Editor baglami
};
