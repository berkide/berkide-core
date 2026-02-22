// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <string>
#include <vector>
#include <unordered_map>

// Represents a single help topic loaded from a markdown file
// Bir markdown dosyasindan yuklenmis tek bir yardim konusunu temsil eder
struct HelpTopic {
    std::string id;          // Topic identifier (filename without extension) / Konu tanimlayicisi (uzantisiz dosya adi)
    std::string title;       // Title from first # heading / Ilk # basliktan alinan baslik
    std::string content;     // Full markdown content / Tam markdown icerigi
    std::vector<std::string> tags;  // Tags from <!-- tags: ... --> comment / <!-- tags: ... --> yorumundan etiketler
};

// Offline wiki/help system that loads markdown files from a directory.
// Bir dizinden markdown dosyalarini yukleyen cevrimdisi wiki/yardim sistemi.
// Like Emacs Info pages - built-in, searchable documentation.
// Emacs Info sayfalari gibi - yerlesik, aranabilir dokumantasyon.
class HelpSystem {
public:
    HelpSystem() = default;

    // Load all .md files from a directory as help topics
    // Bir dizindeki tum .md dosyalarini yardim konusu olarak yukle
    void loadFromDirectory(const std::string& dirPath);

    // Get a topic by its ID
    // Kimligine gore bir konu al
    const HelpTopic* getTopic(const std::string& id) const;

    // List all available topics
    // Tum mevcut konulari listele
    std::vector<const HelpTopic*> listTopics() const;

    // Search topics by query string (matches title, content, tags)
    // Sorgu dizesiyle konulari ara (baslik, icerik, etiketlerle eslesir)
    std::vector<const HelpTopic*> search(const std::string& query) const;

    // Reload all topics from directory
    // Tum konulari dizinden yeniden yukle
    void refresh(const std::string& dirPath);

private:
    // Parse a markdown file into a HelpTopic
    // Bir markdown dosyasini HelpTopic'e ayristir
    HelpTopic parseFile(const std::string& path, const std::string& id) const;

    std::unordered_map<std::string, HelpTopic> topics_;  // id -> topic / kimlik -> konu
};
