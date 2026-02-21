#include "HelpSystem.h"
#include "Logger.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

// Load all .md files from a directory as help topics
// Bir dizindeki tum .md dosyalarini yardim konusu olarak yukle
void HelpSystem::loadFromDirectory(const std::string& dirPath) {
    if (!fs::exists(dirPath)) return;

    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".md") continue;

        std::string id = entry.path().stem().string();
        auto topic = parseFile(entry.path().string(), id);
        topics_[id] = std::move(topic);
    }

    LOG_INFO("[Help] Loaded ", topics_.size(), " topics from ", dirPath);
}

// Parse a single markdown file into a HelpTopic
// Tek bir markdown dosyasini HelpTopic'e ayristir
HelpTopic HelpSystem::parseFile(const std::string& path, const std::string& id) const {
    HelpTopic topic;
    topic.id = id;

    std::ifstream ifs(path);
    if (!ifs.is_open()) return topic;

    std::ostringstream oss;
    oss << ifs.rdbuf();
    topic.content = oss.str();

    // Extract title from first # heading
    // Ilk # basliktan baslik cikar
    std::istringstream stream(topic.content);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.size() > 2 && line[0] == '#' && line[1] == ' ') {
            topic.title = line.substr(2);
            break;
        }
    }

    if (topic.title.empty()) {
        topic.title = id;
    }

    // Extract tags from <!-- tags: tag1, tag2 --> comment
    // <!-- tags: etiket1, etiket2 --> yorumundan etiketleri cikar
    auto tagPos = topic.content.find("<!-- tags:");
    if (tagPos != std::string::npos) {
        auto endPos = topic.content.find("-->", tagPos);
        if (endPos != std::string::npos) {
            std::string tagStr = topic.content.substr(tagPos + 10, endPos - tagPos - 10);
            // Trim and split by comma
            // Kirp ve virgule gore bol
            std::istringstream tagStream(tagStr);
            std::string tag;
            while (std::getline(tagStream, tag, ',')) {
                // Trim whitespace
                auto start = tag.find_first_not_of(" \t");
                auto end = tag.find_last_not_of(" \t");
                if (start != std::string::npos) {
                    topic.tags.push_back(tag.substr(start, end - start + 1));
                }
            }
        }
    }

    return topic;
}

// Get a topic by ID, returns nullptr if not found
// Kimligine gore konu al, bulunamazsa nullptr dondur
const HelpTopic* HelpSystem::getTopic(const std::string& id) const {
    auto it = topics_.find(id);
    if (it == topics_.end()) return nullptr;
    return &it->second;
}

// List all available topics
// Tum mevcut konulari listele
std::vector<const HelpTopic*> HelpSystem::listTopics() const {
    std::vector<const HelpTopic*> result;
    result.reserve(topics_.size());
    for (auto& [id, topic] : topics_) {
        result.push_back(&topic);
    }
    // Sort by title
    // Basliga gore sirala
    std::sort(result.begin(), result.end(), [](const HelpTopic* a, const HelpTopic* b) {
        return a->title < b->title;
    });
    return result;
}

// Search topics by query string (case-insensitive match on title, content, tags)
// Sorgu dizesiyle konulari ara (baslik, icerik, etiketlerde buyuk/kucuk harf duyarsiz esleme)
std::vector<const HelpTopic*> HelpSystem::search(const std::string& query) const {
    std::vector<const HelpTopic*> result;
    if (query.empty()) return listTopics();

    // Convert query to lowercase for case-insensitive search
    // Buyuk/kucuk harf duyarsiz arama icin sorguyu kucuk harfe cevir
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (auto& [id, topic] : topics_) {
        // Check title
        std::string lowerTitle = topic.title;
        std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), ::tolower);
        if (lowerTitle.find(lowerQuery) != std::string::npos) {
            result.push_back(&topic);
            continue;
        }

        // Check tags
        bool tagMatch = false;
        for (auto& tag : topic.tags) {
            std::string lowerTag = tag;
            std::transform(lowerTag.begin(), lowerTag.end(), lowerTag.begin(), ::tolower);
            if (lowerTag.find(lowerQuery) != std::string::npos) {
                tagMatch = true;
                break;
            }
        }
        if (tagMatch) {
            result.push_back(&topic);
            continue;
        }

        // Check content
        std::string lowerContent = topic.content;
        std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::tolower);
        if (lowerContent.find(lowerQuery) != std::string::npos) {
            result.push_back(&topic);
        }
    }

    return result;
}

// Reload all topics from directory
// Tum konulari dizinden yeniden yukle
void HelpSystem::refresh(const std::string& dirPath) {
    topics_.clear();
    loadFromDirectory(dirPath);
}
