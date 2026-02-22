// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once

#ifdef BERKIDE_TREESITTER_ENABLED

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>

// Forward declare tree-sitter C types
// Tree-sitter C turlerini on bildir
extern "C" {
    typedef struct TSParser TSParser;
    typedef struct TSTree TSTree;
    typedef struct TSQuery TSQuery;
    typedef struct TSQueryCursor TSQueryCursor;
    typedef struct TSLanguage TSLanguage;
}

// A single node in the syntax tree
// Soz dizimi agacindaki tek bir dugum
struct SyntaxNode {
    std::string type;            // Node type (e.g., "function_definition") / Dugum turu
    int startLine = 0;           // Start line / Baslangic satiri
    int startCol  = 0;           // Start column / Baslangic sutunu
    int endLine   = 0;           // End line / Bitis satiri
    int endCol    = 0;           // End column / Bitis sutunu
    bool isNamed = false;        // Whether the node is named / Dugumun adlandirilmis olup olmadigi
    std::string fieldName;       // Field name if applicable / Varsa alan adi
    std::vector<SyntaxNode> children;  // Child nodes / Cocuk dugumler
};

// A query match capture
// Sorgu esleme yakalamasi
struct QueryCapture {
    std::string name;            // Capture name (e.g., "@function.name") / Yakalama adi
    std::string text;            // Captured text / Yakalanan metin
    int startLine = 0;
    int startCol  = 0;
    int endLine   = 0;
    int endCol    = 0;
};

// A query match result (one pattern match with all captures)
// Sorgu esleme sonucu (tum yakalamalarla bir kalip eslemesi)
struct QueryMatch {
    int patternIndex = 0;
    std::vector<QueryCapture> captures;
};

// C++ wrapper around tree-sitter for syntax parsing.
// Soz dizimi ayristirma icin tree-sitter etrafinda C++ sarmalayici.
// Supports incremental parsing, queries, and dynamic language loading.
// Artimsal ayristirma, sorgular ve dinamik dil yuklemeyi destekler.
class TreeSitterEngine {
public:
    TreeSitterEngine();
    ~TreeSitterEngine();

    // Non-copyable
    // Kopyalanamaz
    TreeSitterEngine(const TreeSitterEngine&) = delete;
    TreeSitterEngine& operator=(const TreeSitterEngine&) = delete;

    // Load a language grammar from a shared library (.so/.dylib/.dll)
    // Paylasimli kutuphaneden (.so/.dylib/.dll) bir dil grameri yukle
    // The library must export a function: const TSLanguage* tree_sitter_{name}()
    // Kutuphane su fonksiyonu disari aktarmalidir: const TSLanguage* tree_sitter_{name}()
    bool loadLanguage(const std::string& name, const std::string& libraryPath);

    // Check if a language is loaded
    // Bir dilin yuklu olup olmadigini kontrol et
    bool hasLanguage(const std::string& name) const;

    // List loaded language names
    // Yuklu dil adlarini listele
    std::vector<std::string> listLanguages() const;

    // Set the language for the parser
    // Ayristirici icin dili ayarla
    bool setLanguage(const std::string& name);

    // Get current language name
    // Mevcut dil adini al
    const std::string& currentLanguage() const { return currentLang_; }

    // Parse source code (full parse or reparse after edit)
    // Kaynak kodu ayristir (tam ayristirma veya duzenleme sonrasi yeniden ayristirma)
    bool parse(const std::string& source);

    // Apply an edit and reparse (incremental)
    // Bir duzenleme uygula ve yeniden ayristir (artimsal)
    bool editAndReparse(
        int startLine, int startCol,
        int oldEndLine, int oldEndCol,
        int newEndLine, int newEndCol,
        const std::string& newSource);

    // Check if a tree exists (has been parsed)
    // Agacin var olup olmadigini kontrol et (ayristirilmis mi)
    bool hasTree() const;

    // Get the root node of the syntax tree
    // Soz dizimi agacinin kok dugumunu al
    SyntaxNode rootNode() const;

    // Get the node at a specific position
    // Belirli bir konumdaki dugumu al
    SyntaxNode nodeAt(int line, int col) const;

    // Get the named node at a position (skip anonymous nodes)
    // Konumdaki adlandirilmis dugumu al (anonim dugumleri atla)
    SyntaxNode namedNodeAt(int line, int col) const;

    // Run a query on the current tree
    // Mevcut agac uzerinde bir sorgu calistir
    std::vector<QueryMatch> query(const std::string& queryStr, const std::string& source,
                                   int startLine = 0, int endLine = -1) const;

    // Get all syntax errors in the tree
    // Agactaki tum soz dizimi hatalarini al
    std::vector<SyntaxNode> errors() const;

    // Release the current tree
    // Mevcut agaci serbest birak
    void reset();

private:
    mutable std::mutex mutex_;
    TSParser* parser_ = nullptr;
    TSTree* tree_ = nullptr;
    std::string currentLang_;
    std::string lastSource_;  // Keep last source for incremental edit / Artimsal duzenleme icin son kaynagi tut

    // Language registry: name -> (TSLanguage*, dlhandle)
    // Dil kayit defteri: ad -> (TSLanguage*, dlhandle)
    struct LangEntry {
        const TSLanguage* lang = nullptr;
        void* dlHandle = nullptr;
    };
    std::unordered_map<std::string, LangEntry> languages_;

    // Helper: convert TSNode to SyntaxNode
    // Yardimci: TSNode'u SyntaxNode'a cevir
    SyntaxNode convertNode(void* tsNode, int depth = 0, int maxDepth = 100) const;

    // Helper: collect error nodes recursively
    // Yardimci: hata dugumlerini ozyinelemeli olarak topla
    void collectErrors(void* tsNode, std::vector<SyntaxNode>& errors) const;
};

#endif // BERKIDE_TREESITTER_ENABLED
