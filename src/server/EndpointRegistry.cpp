#include "EndpointRegistry.h"

// Register a GET endpoint with metadata and httplib route
// Metadata ve httplib rotasi ile bir GET endpoint'i kaydet
void EndpointRegistry::get(httplib::Server* srv,
                           const std::string& path,
                           const std::string& desc,
                           bool auth,
                           httplib::Server::Handler handler,
                           json params) {
    endpoints_.push_back({"GET", path, desc, auth, std::move(params), json::object()});
    srv->Get(path.c_str(), wrapAuth(auth, std::move(handler)));
}

// Register a POST endpoint with metadata and httplib route
// Metadata ve httplib rotasi ile bir POST endpoint'i kaydet
void EndpointRegistry::post(httplib::Server* srv,
                            const std::string& path,
                            const std::string& desc,
                            bool auth,
                            httplib::Server::Handler handler,
                            json body) {
    endpoints_.push_back({"POST", path, desc, auth, json::object(), std::move(body)});
    srv->Post(path.c_str(), wrapAuth(auth, std::move(handler)));
}

// Wrap handler with auth check if required, otherwise pass through
// Gerekirse handler'i auth kontrolu ile sar, degilse dogrudan gecir
httplib::Server::Handler EndpointRegistry::wrapAuth(bool auth, httplib::Server::Handler handler) const {
    if (!auth || !authChecker_) return handler;

    auto checker = authChecker_;
    return [checker, handler = std::move(handler)](const httplib::Request& req, httplib::Response& res) {
        if (!checker(req, res)) return;
        handler(req, res);
    };
}

// Serialize all endpoints to a JSON array for API discovery
// API kesfetme icin tum endpoint'leri JSON dizisine cevir
json EndpointRegistry::toJson() const {
    json arr = json::array();
    for (const auto& ep : endpoints_) {
        json entry = {
            {"method", ep.method},
            {"path", ep.path},
            {"description", ep.description},
            {"auth", ep.authRequired}
        };
        if (!ep.params.empty()) entry["params"] = ep.params;
        if (!ep.body.empty()) entry["body"] = ep.body;
        arr.push_back(std::move(entry));
    }
    return arr;
}
