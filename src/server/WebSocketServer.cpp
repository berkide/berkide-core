// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "WebSocketServer.h"
#include "StateSnapshot.h"
#include "buffers.h"
#include "EventBus.h"
#include "V8Engine.h"
#include "Logger.h"

// Default constructor for WebSocket server
// WebSocket sunucusu icin varsayilan kurucu
WebSocketServer::WebSocketServer() = default;

// Destructor stops the server before cleanup
// Yikici, temizlik oncesinde sunucuyu durdurur
WebSocketServer::~WebSocketServer() { stop(); }

// Validate authentication token from the WebSocket connection URL query string
// WebSocket baglanti URL'sindeki sorgu dizesinden kimlik dogrulama tokenini dogrula
bool WebSocketServer::validateToken(const std::string& url) const {
    if (!config_.requireAuth) return true;

    auto pos = url.find("token=");
    if (pos == std::string::npos) return false;

    std::string token = url.substr(pos + 6);
    auto ampPos = token.find('&');
    if (ampPos != std::string::npos) token = token.substr(0, ampPos);

    return token == config_.bearerToken;
}

// Broadcast a named event with JSON data to all connected WebSocket clients
// Tum bagli WebSocket istemcilerine adlandirilmis olayi JSON verisiyle yayinla
void WebSocketServer::broadcastEvent(const std::string& event, const json& data) {
    json msg = {{"event", event}, {"data", data}};
    broadcast(msg.dump());
}

// Subscribe to EventBus events (buffer, cursor, tab changes) for real-time client sync
// Gercek zamanli istemci senkronizasyonu icin EventBus olaylarina (tampon, imlecc, sekme) abone ol
void WebSocketServer::setupEventBusListeners() {
    if (!edCtx_ || !edCtx_->eventBus) return;

    auto* eb = edCtx_->eventBus;

    // Listen for buffer changes
    eb->on("bufferChanged", [this](const EventBus::Event& e) {
        if (!edCtx_ || !edCtx_->buffers) return;
        auto& cur = edCtx_->buffers->active().getCursor();
        broadcastEvent("bufferChanged", {
            {"filePath", e.payload},
            {"cursor", {{"line", cur.getLine()}, {"col", cur.getCol()}}}
        });
    });

    // Listen for cursor moves
    eb->on("cursorMoved", [this](const EventBus::Event& e) {
        if (!edCtx_ || !edCtx_->buffers) return;
        auto& cur = edCtx_->buffers->active().getCursor();
        broadcastEvent("cursorMoved", {
            {"line", cur.getLine()}, {"col", cur.getCol()}
        });
    });

    // Listen for tab changes
    eb->on("tabChanged", [this](const EventBus::Event& e) {
        if (!edCtx_ || !edCtx_->buffers) return;
        broadcastEvent("tabChanged", {
            {"activeIndex", (int)edCtx_->buffers->activeIndex()}
        });
    });
}

// Start WebSocket server on given port with default configuration
// Verilen portta varsayilan yapilandirma ile WebSocket sunucusunu baslat
void WebSocketServer::start(int port)
{
    ServerConfig cfg;
    cfg.wsPort = port;
    start(cfg);
}

// Start WebSocket server with full config, handle client connections, commands, and sync requests
// Tam yapilandirma ile WebSocket sunucusunu baslat, istemci baglantilari, komutlar ve senk isteklerini yonet
void WebSocketServer::start(const ServerConfig& config)
{
    if (running_) return;
    config_ = config;
    running_ = true;

    server_ = std::make_unique<ix::WebSocketServer>(config_.wsPort, config_.bindAddress);

#ifdef BERKIDE_TLS_ENABLED
    // Apply TLS options if enabled
    // TLS etkinse TLS seceneklerini uygula
    if (config_.tlsEnabled) {
        ix::SocketTLSOptions tlsOpts;
        tlsOpts.tls = true;
        tlsOpts.certFile = config_.tlsCertFile;
        tlsOpts.keyFile  = config_.tlsKeyFile;
        tlsOpts.caFile   = config_.tlsCaFile;
        server_->setTLSOptions(tlsOpts);
        LOG_INFO("[WS] TLS enabled");
    }
#endif

    server_->setOnClientMessageCallback(
        [this](std::shared_ptr<ix::ConnectionState> connectionState,
               ix::WebSocket& ws,
               const ix::WebSocketMessagePtr& msg)
        {
            if (msg->type == ix::WebSocketMessageType::Open)
            {
                if (!validateToken(msg->openInfo.uri)) {
                    LOG_WARN("[WS] Unauthorized connection attempt, closing.");
                    ws.close();
                    return;
                }

                std::lock_guard<std::mutex> lock(clientsMutex_);
                clients_.insert(&ws);
                LOG_INFO("[WS] Client connected");

                // Send initial full state sync
                if (edCtx_ && edCtx_->buffers) {
                    json state = StateSnapshot::fullState(*edCtx_->buffers);
                    json msg = {{"event", "fullSync"}, {"data", state}};
                    ws.send(msg.dump());
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Message)
            {
                json body = json::parse(msg->str, nullptr, false);
                if (body.is_object() && body.contains("cmd"))
                {
                    std::string cmd = body.value("cmd", "");
                    json args = body.value("args", json::object());

                    json result = V8Engine::instance().dispatchCommand(cmd, args);
                    ws.send(result.dump());
                }
                else if (body.is_object() && body.contains("action"))
                {
                    // Direct buffer edit via WS
                    std::string action = body.value("action", "");
                    if (action == "requestSync" && edCtx_ && edCtx_->buffers) {
                        json state = StateSnapshot::fullState(*edCtx_->buffers);
                        json resp = {{"event", "fullSync"}, {"data", state}};
                        ws.send(resp.dump());
                    }
                }
                else
                {
                    ws.send("Echo: " + msg->str);
                }
            }
            else if (msg->type == ix::WebSocketMessageType::Close)
            {
                std::lock_guard<std::mutex> lock(clientsMutex_);
                clients_.erase(&ws);
                LOG_INFO("[WS] Client disconnected");
            }
        });

    // Setup EventBus listeners for real-time sync
    setupEventBusListeners();

    auto res = server_->listen();
    if (!res.first)
    {
        LOG_ERROR("[WS] Failed to listen on ", config_.bindAddress, ":", config_.wsPort, ": ", res.second);
        running_ = false;
        return;
    }

    server_->start();
    LOG_INFO("[WS] Listening on ws://", config_.bindAddress, ":", config_.wsPort);
}

// Stop the WebSocket server and clear all connected clients
// WebSocket sunucusunu durdur ve tum bagli istemcileri temizle
void WebSocketServer::stop()
{
    if (!running_) return;
    running_ = false;

    if (server_)
    {
        server_->stop();
        LOG_INFO("[WS] Server stopped");
    }

    std::lock_guard<std::mutex> lock(clientsMutex_);
    clients_.clear();
}

// Send a message string to all connected WebSocket clients
// Tum bagli WebSocket istemcilerine bir mesaj dizesi gonder
void WebSocketServer::broadcast(const std::string& msg)
{
    if (!running_ || !server_) return;

    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto* ws : clients_)
    {
        if (ws)
            ws->send(msg);
    }
}
