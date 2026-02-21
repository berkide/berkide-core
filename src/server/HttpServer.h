#pragma once
#include <atomic>
#include <thread>
#include "http/httplib.h"
#include "nlohmann/json.hpp"
#include "ServerConfig.h"
#include "EditorContext.h"
#include "EndpointRegistry.h"

using json = nlohmann::json;

// HTTP REST API server for headless editor access.
// Basliksiz editor erisimi icin HTTP REST API sunucusu.
// Provides endpoints for buffer operations, cursor control, state queries, and command dispatch.
// Buffer islemleri, imlec kontrolu, durum sorgulari ve komut dagitimi icin endpoint'ler saglar.
class HttpServer {
public:
    HttpServer();
    ~HttpServer();

    // Set the editor context for accessing real editor objects
    // Gercek editor nesnelerine erismek icin editor baglamini ayarla
    void setEditorContext(EditorContext* ctx) { edCtx_ = ctx; }

    // Start the server on a specific port (uses default config)
    // Sunucuyu belirli bir portta baslat (varsayilan yapilandirmayi kullanir)
    bool start(int port = 1881);

    // Start the server with full configuration
    // Sunucuyu tam yapilandirmayla baslat
    bool start(const ServerConfig& config);

    // Stop the server and join the thread
    // Sunucuyu durdur ve thread'i birlesir
    void stop();

    // Check if the server is currently running
    // Sunucunun su anda calisip calismadigini kontrol et
    bool isRunning() const { return running_; }

    // Get the endpoint registry (read-only) for discovery
    // Kesfetme icin endpoint kayit defterini al (salt okunur)
    const EndpointRegistry& registry() const { return registry_; }

private:
    // Register all REST API routes via the central endpoint registry
    // Tum REST API rotalarini merkezi endpoint kayit defteri uzerinden kaydet
    void setupRoutes();

    // Validate Bearer token authentication on a request
    // Bir istekte Bearer token kimlik dogrulamasini dogrula
    bool checkAuth(const httplib::Request& req, httplib::Response& res) const;

    // Get the active server instance (either plain or SSL)
    // Aktif sunucu ornegini al (duz veya SSL)
    httplib::Server* activeServer() const {
#ifdef BERKIDE_TLS_ENABLED
        if (sslServer_) return sslServer_.get();
#endif
        return server_.get();
    }

    std::atomic<bool> running_{false};                  // Server running state / Sunucu calisma durumu
    std::unique_ptr<std::thread> serverThread_;          // Server thread / Sunucu thread'i
    std::unique_ptr<httplib::Server> server_;             // HTTP server instance / HTTP sunucu ornegi
#ifdef BERKIDE_TLS_ENABLED
    std::unique_ptr<httplib::SSLServer> sslServer_;      // HTTPS server instance / HTTPS sunucu ornegi
#endif
    ServerConfig config_;                                 // Server configuration / Sunucu yapilandirmasi
    EditorContext* edCtx_ = nullptr;                      // Editor context for accessing core / Core'a erismek icin editor baglami
    EndpointRegistry registry_;                           // Central endpoint registry / Merkezi endpoint kayit defteri
};
