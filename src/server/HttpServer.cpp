#include "HttpServer.h"
#include "StateSnapshot.h"
#include "buffers.h"
#include "HelpSystem.h"
#include "Logger.h"
#include "V8Engine.h"

// Default constructor for HTTP server
// HTTP sunucusu icin varsayilan kurucu
HttpServer::HttpServer() = default;

// Destructor stops the server gracefully before cleanup
// Yikici, temizlik oncesinde sunucuyu duzgun bir sekilde durdurur
HttpServer::~HttpServer() {
    stop();
}

// Check Bearer token authentication on incoming request
// Gelen istekte Bearer token kimlik dogrulamasini kontrol et
bool HttpServer::checkAuth(const httplib::Request& req, httplib::Response& res) const {
    if (!config_.requireAuth) return true;

    std::string authHeader = req.get_header_value("Authorization");
    std::string expected = "Bearer " + config_.bearerToken;
    if (authHeader == expected) return true;

    res.status = 401;
    res.set_content("{\"error\":\"unauthorized\"}", "application/json");
    return false;
}

// Register all HTTP API routes via the central endpoint registry
// Tum HTTP API rotalarini merkezi endpoint kayit defteri uzerinden kaydet
void HttpServer::setupRoutes() {
    auto* srv = activeServer();

    // Configure auth checker for the registry
    // Kayit defteri icin kimlik dogrulama kontrolcusunu yapilandir
    registry_.setAuthChecker([this](const httplib::Request& req, httplib::Response& res) {
        return checkAuth(req, res);
    });

    // --- Discovery & Health ---

    registry_.get(srv, "/ping", "Health check ping", false,
        [](const httplib::Request&, httplib::Response& res) {
            res.set_content("pong", "text/plain");
        });

    registry_.get(srv, "/api/endpoints", "List all API endpoints with metadata", false,
        [this](const httplib::Request&, httplib::Response& res) {
            res.set_content(registry_.toJson().dump(2), "application/json");
        });

    registry_.get(srv, "/api/server", "Server status and configuration", false,
        [this](const httplib::Request&, httplib::Response& res) {
            json info = {
                {"name", "BerkIDE"},
                {"version", BERKIDE_VERSION},
                {"status", running_ ? "running" : "stopped"},
                {"http", {{"bind", config_.bindAddress}, {"port", config_.httpPort}}},
                {"ws", {{"port", config_.wsPort}}},
                {"tls", config_.tlsEnabled},
                {"auth", config_.requireAuth},
                {"endpoints", registry_.count()}
            };
            res.set_content(info.dump(2), "application/json");
        });

    // --- Editor State API ---

    registry_.get(srv, "/api/state", "Full editor state (cursor, buffer, mode, open buffers)", true,
        [this](const httplib::Request&, httplib::Response& res) {
            if (!edCtx_ || !edCtx_->buffers) { res.status = 500; return; }
            json state = StateSnapshot::fullState(*edCtx_->buffers);
            res.set_content(state.dump(), "application/json");
        });

    registry_.get(srv, "/api/buffer", "Active buffer content as array of lines", true,
        [this](const httplib::Request&, httplib::Response& res) {
            if (!edCtx_ || !edCtx_->buffers) { res.status = 500; return; }
            json buf = StateSnapshot::activeBuffer(*edCtx_->buffers);
            res.set_content(buf.dump(), "application/json");
        });

    registry_.get(srv, R"(/api/buffer/line/(\d+))", "Get a single line by number", true,
        [this](const httplib::Request& req, httplib::Response& res) {
            if (!edCtx_ || !edCtx_->buffers) { res.status = 500; return; }
            int n = std::stoi(req.matches[1]);
            json line = StateSnapshot::bufferLine(*edCtx_->buffers, n);
            res.set_content(line.dump(), "application/json");
        },
        json({{"n", "integer — line number (0-based)"}}));

    registry_.get(srv, "/api/cursor", "Current cursor position {line, col}", true,
        [this](const httplib::Request&, httplib::Response& res) {
            if (!edCtx_ || !edCtx_->buffers) { res.status = 500; return; }
            json cur = StateSnapshot::cursorPosition(*edCtx_->buffers);
            res.set_content(cur.dump(), "application/json");
        });

    registry_.get(srv, "/api/buffers", "List all open buffers with titles and active flag", true,
        [this](const httplib::Request&, httplib::Response& res) {
            if (!edCtx_ || !edCtx_->buffers) { res.status = 500; return; }
            json list = StateSnapshot::bufferList(*edCtx_->buffers);
            res.set_content(list.dump(), "application/json");
        });

    // --- Input API ---

    registry_.post(srv, "/api/input/key", "Simulate key press via command dispatch", true,
        [this](const httplib::Request& req, httplib::Response& res) {
            auto body = json::parse(req.body, nullptr, false);
            if (!body.is_object()) { res.status = 400; return; }
            json result = V8Engine::instance().dispatchCommand("input.key", body);
            res.set_content(result.dump(), "application/json");
        },
        json({{"key", "string — key name (e.g. 'Enter', 'Escape', 'a')"}}));

    registry_.post(srv, "/api/input/char", "Insert character into active buffer", true,
        [this](const httplib::Request& req, httplib::Response& res) {
            auto body = json::parse(req.body, nullptr, false);
            if (!body.is_object() || body.value("text", "").empty()) { res.status = 400; return; }
            json result = V8Engine::instance().dispatchCommand("input.char", body);
            res.set_content(result.dump(), "application/json");
        },
        json({{"text", "string — character(s) to insert"}}));

    // --- Buffer Edit API ---

    registry_.post(srv, "/api/buffer/edit", "Buffer edit operations (insert, delete, insertLine, deleteLine)", true,
        [this](const httplib::Request& req, httplib::Response& res) {
            auto body = json::parse(req.body, nullptr, false);
            if (!body.is_object()) { res.status = 400; return; }

            std::string action = body.value("action", "");

            // Map action names to registered command names
            // Islem adlarini kayitli komut adlarina esle
            std::string cmd;
            if (action == "insert")          cmd = "buffer.insert";
            else if (action == "delete")     cmd = "buffer.delete";
            else if (action == "insertLine") cmd = "buffer.splitLine";
            else if (action == "deleteLine") cmd = "edit.deleteLine";
            else {
                res.status = 400;
                res.set_content(json({{"ok", false}, {"error", "unknown action"}}).dump(), "application/json");
                return;
            }

            json result = V8Engine::instance().dispatchCommand(cmd, body);
            res.set_content(result.dump(), "application/json");
        },
        json({{"action", "string — insert|delete|insertLine|deleteLine"},
              {"text", "string — text to insert (for insert action)"},
              {"line", "integer — target line number"},
              {"col", "integer — target column number"}}));

    registry_.post(srv, "/api/buffer/open", "Open a file into a new buffer", true,
        [this](const httplib::Request& req, httplib::Response& res) {
            auto body = json::parse(req.body, nullptr, false);
            if (!body.is_object() || body.value("path", "").empty()) { res.status = 400; return; }
            json result = V8Engine::instance().dispatchCommand("file.open", body);
            res.set_content(result.dump(), "application/json");
        },
        json({{"path", "string — file path to open"}}));

    registry_.post(srv, "/api/buffer/save", "Save active buffer to disk", true,
        [this](const httplib::Request&, httplib::Response& res) {
            json result = V8Engine::instance().dispatchCommand("file.save", {});
            res.set_content(result.dump(), "application/json");
        });

    registry_.post(srv, "/api/buffer/close", "Close active buffer", true,
        [this](const httplib::Request&, httplib::Response& res) {
            json result = V8Engine::instance().dispatchCommand("tab.close", {});
            res.set_content(result.dump(), "application/json");
        });

    registry_.post(srv, "/api/buffers/switch", "Switch to a buffer by index", true,
        [this](const httplib::Request& req, httplib::Response& res) {
            auto body = json::parse(req.body, nullptr, false);
            if (!body.is_object() || body.value("index", -1) < 0) { res.status = 400; return; }
            json result = V8Engine::instance().dispatchCommand("tab.switchTo", body);
            res.set_content(result.dump(), "application/json");
        },
        json({{"index", "integer — buffer index to switch to"}}));

    // --- Command Dispatch API ---

    registry_.post(srv, "/command", "Legacy command dispatch", true,
        [this](const httplib::Request& req, httplib::Response& res) {
            auto jsonBody = json::parse(req.body, nullptr, false);
            std::string cmd = jsonBody.value("cmd", "");
            auto args = jsonBody.value("args", json::object());
            json result = V8Engine::instance().dispatchCommand(cmd, args);
            res.set_content(result.dump(), "application/json");
        },
        json({{"cmd", "string — command name"}, {"args", "object — command arguments"}}));

    registry_.post(srv, "/api/command", "Unified command and query dispatch (returns result data)", true,
        [this](const httplib::Request& req, httplib::Response& res) {
            auto jsonBody = json::parse(req.body, nullptr, false);
            std::string cmd = jsonBody.value("cmd", "");
            auto args = jsonBody.value("args", json::object());
            json result = V8Engine::instance().dispatchCommand(cmd, args);
            res.set_content(result.dump(), "application/json");
        },
        json({{"cmd", "string — command name"}, {"args", "object — command arguments (optional)"}}));

    registry_.get(srv, "/api/commands", "List all registered commands and queries", true,
        [this](const httplib::Request&, httplib::Response& res) {
            json list = V8Engine::instance().listCommands();
            res.set_content(list.dump(), "application/json");
        });

    // --- Help System API ---

    registry_.get(srv, "/api/help", "List all help topics", true,
        [this](const httplib::Request&, httplib::Response& res) {
            if (!edCtx_ || !edCtx_->helpSystem) { res.status = 500; return; }
            auto topics = edCtx_->helpSystem->listTopics();
            json arr = json::array();
            for (auto* t : topics) {
                arr.push_back({{"id", t->id}, {"title", t->title}, {"tags", t->tags}});
            }
            res.set_content(arr.dump(), "application/json");
        });

    registry_.get(srv, "/api/help/search", "Search help topics by keyword", true,
        [this](const httplib::Request& req, httplib::Response& res) {
            if (!edCtx_ || !edCtx_->helpSystem) { res.status = 500; return; }
            std::string query = req.get_param_value("q");
            auto results = edCtx_->helpSystem->search(query);
            json arr = json::array();
            for (auto* t : results) {
                arr.push_back({{"id", t->id}, {"title", t->title}, {"tags", t->tags}});
            }
            res.set_content(arr.dump(), "application/json");
        },
        json({{"q", "string — search query"}}));

    registry_.get(srv, R"(/api/help/([a-zA-Z0-9_-]+))", "Get a specific help topic with full content", true,
        [this](const httplib::Request& req, httplib::Response& res) {
            if (!edCtx_ || !edCtx_->helpSystem) { res.status = 500; return; }
            std::string topicId = req.matches[1];
            auto* topic = edCtx_->helpSystem->getTopic(topicId);
            if (!topic) {
                res.status = 404;
                res.set_content(json({{"error", "topic not found"}}).dump(), "application/json");
                return;
            }
            json j = {
                {"id", topic->id},
                {"title", topic->title},
                {"content", topic->content},
                {"tags", topic->tags}
            };
            res.set_content(j.dump(), "application/json");
        },
        json({{"topic", "string — help topic ID"}}));

    LOG_INFO("[HTTP] ", registry_.count(), " endpoints registered via EndpointRegistry");
}

// Start HTTP server on given port with default configuration
// Verilen portta varsayilan yapilandirma ile HTTP sunucusunu baslat
bool HttpServer::start(int port) {
    ServerConfig cfg;
    cfg.httpPort = port;
    return start(cfg);
}

// Start HTTP server with full configuration, setup routes and launch listener thread
// Tam yapilandirma ile HTTP sunucusunu baslat, rotalari kur ve dinleyici is parcacigini calistir
bool HttpServer::start(const ServerConfig& config) {
    if (running_) {
        LOG_WARN("[HTTP] Already running.");
        return false;
    }

    config_ = config;
    running_ = true;

#ifdef BERKIDE_TLS_ENABLED
    if (config_.tlsEnabled) {
        sslServer_ = std::make_unique<httplib::SSLServer>(
            config_.tlsCertFile.c_str(), config_.tlsKeyFile.c_str());
        LOG_INFO("[HTTP] TLS enabled");
    } else {
        server_ = std::make_unique<httplib::Server>();
    }
#else
    server_ = std::make_unique<httplib::Server>();
#endif

    setupRoutes();

    serverThread_ = std::make_unique<std::thread>([this]() {
        std::string proto = "http";
#ifdef BERKIDE_TLS_ENABLED
        if (config_.tlsEnabled) proto = "https";
#endif
        LOG_INFO("[HTTP] Listening on ", proto, "://", config_.bindAddress, ":", config_.httpPort, "...");

#ifdef BERKIDE_TLS_ENABLED
        if (sslServer_) {
            sslServer_->listen(config_.bindAddress, config_.httpPort);
        } else
#endif
        {
            server_->listen(config_.bindAddress, config_.httpPort);
        }
    });

    return true;
}

// Stop the HTTP server and join the listener thread
// HTTP sunucusunu durdur ve dinleyici is parcacigini bekleyerek kapat
void HttpServer::stop() {
    if (!running_) return;
    running_ = false;

    auto* srv = activeServer();
    if (srv) {
        srv->stop();
        LOG_INFO("[HTTP] Server stopped.");
    }

    if (serverThread_ && serverThread_->joinable()) {
        serverThread_->join();
        LOG_INFO("[HTTP] Thread joined.");
    }

    serverThread_.reset();
    server_.reset();
#ifdef BERKIDE_TLS_ENABLED
    sslServer_.reset();
#endif
}
