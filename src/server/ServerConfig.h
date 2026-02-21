#pragma once
#include <string>

// Configuration for HTTP and WebSocket servers.
// HTTP ve WebSocket sunuculari icin yapilandirma.
// Controls bind address, ports, authentication, and TLS settings.
// Baglanti adresi, portlar, kimlik dogrulama ve TLS ayarlarini kontrol eder.
struct ServerConfig {
    std::string bindAddress = "127.0.0.1";  // Bind address (127.0.0.1 = local only, 0.0.0.0 = all interfaces) / Baglanti adresi
    int httpPort = 1881;                     // HTTP REST API port / HTTP REST API portu
    int wsPort   = 1882;                     // WebSocket port / WebSocket portu
    bool requireAuth = false;                // Whether authentication is required / Kimlik dogrulamanin gerekli olup olmadigi
    std::string bearerToken;                 // Bearer token for HTTP, query param for WS / HTTP icin Bearer token, WS icin sorgu parametresi

    // TLS/SSL settings (requires BERKIDE_USE_TLS build option)
    // TLS/SSL ayarlari (BERKIDE_USE_TLS derleme secenegi gerektirir)
    bool tlsEnabled = false;                 // Whether TLS is enabled / TLS'in etkin olup olmadigi
    std::string tlsCertFile;                 // Path to TLS certificate file / TLS sertifika dosyasinin yolu
    std::string tlsKeyFile;                  // Path to TLS private key file / TLS ozel anahtar dosyasinin yolu
    std::string tlsCaFile = "NONE";          // Path to CA file (or "NONE") / CA dosyasinin yolu (veya "NONE")
};
