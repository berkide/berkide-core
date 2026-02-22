// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#ifdef BERKIDE_TREESITTER_ENABLED

#include "TreeSitterEngine.h"
#include "Logger.h"
#include <tree_sitter/api.h>

// Platform-specific dynamic library loading
// Platforma ozgu dinamik kutuphane yukleme
#ifdef _WIN32
    #include <windows.h>
    #define DL_OPEN(path) LoadLibraryA(path)
    #define DL_SYM(handle, name) (void*)GetProcAddress((HMODULE)handle, name)
    #define DL_CLOSE(handle) FreeLibrary((HMODULE)handle)
    #define DL_ERROR() "LoadLibrary failed"
#else
    #include <dlfcn.h>
    #define DL_OPEN(path) dlopen(path, RTLD_LAZY)
    #define DL_SYM(handle, name) dlsym(handle, name)
    #define DL_CLOSE(handle) dlclose(handle)
    #define DL_ERROR() dlerror()
#endif

// Constructor: create parser
// Kurucu: ayristirici olustur
TreeSitterEngine::TreeSitterEngine() {
    parser_ = ts_parser_new();
}

// Destructor: clean up all resources
// Yikici: tum kaynaklari temizle
TreeSitterEngine::~TreeSitterEngine() {
    if (tree_) ts_tree_delete(tree_);
    if (parser_) ts_parser_delete(parser_);

    // Close all loaded language libraries
    // Tum yuklu dil kutuphanelerini kapat
    for (auto& [name, entry] : languages_) {
        if (entry.dlHandle) {
            DL_CLOSE(entry.dlHandle);
        }
    }
}

// Load language from shared library
// Paylasimli kutuphaneden dil yukle
bool TreeSitterEngine::loadLanguage(const std::string& name, const std::string& libraryPath) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (languages_.count(name) > 0) return true;  // Already loaded / Zaten yuklu

    void* handle = DL_OPEN(libraryPath.c_str());
    if (!handle) {
        LOG_ERROR("[TreeSitter] Failed to load library: ", libraryPath, " - ", DL_ERROR());
        return false;
    }

    // Look for tree_sitter_{name} function
    // tree_sitter_{name} fonksiyonunu ara
    std::string funcName = "tree_sitter_" + name;
    using LangFunc = const TSLanguage* (*)();
    auto func = reinterpret_cast<LangFunc>(DL_SYM(handle, funcName.c_str()));

    if (!func) {
        LOG_ERROR("[TreeSitter] Function not found: ", funcName, " in ", libraryPath);
        DL_CLOSE(handle);
        return false;
    }

    const TSLanguage* lang = func();
    if (!lang) {
        LOG_ERROR("[TreeSitter] Function returned null: ", funcName);
        DL_CLOSE(handle);
        return false;
    }

    languages_[name] = {lang, handle};
    LOG_INFO("[TreeSitter] Loaded language: ", name, " from ", libraryPath);
    return true;
}

// Check if language is loaded
// Dilin yuklu olup olmadigini kontrol et
bool TreeSitterEngine::hasLanguage(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return languages_.count(name) > 0;
}

// List loaded languages
// Yuklu dilleri listele
std::vector<std::string> TreeSitterEngine::listLanguages() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(languages_.size());
    for (auto& [name, entry] : languages_) {
        result.push_back(name);
    }
    return result;
}

// Set language for parser
// Ayristirici icin dili ayarla
bool TreeSitterEngine::setLanguage(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = languages_.find(name);
    if (it == languages_.end()) {
        LOG_ERROR("[TreeSitter] Language not loaded: ", name);
        return false;
    }

    if (!ts_parser_set_language(parser_, it->second.lang)) {
        LOG_ERROR("[TreeSitter] Failed to set language: ", name);
        return false;
    }

    currentLang_ = name;

    // Clear existing tree since language changed
    // Dil degistigi icin mevcut agaci temizle
    if (tree_) {
        ts_tree_delete(tree_);
        tree_ = nullptr;
    }
    lastSource_.clear();

    return true;
}

// Full parse
// Tam ayristirma
bool TreeSitterEngine::parse(const std::string& source) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!parser_) return false;

    TSTree* newTree = ts_parser_parse_string(
        parser_,
        tree_,  // Old tree for incremental (or null for full)
        source.c_str(),
        static_cast<uint32_t>(source.size())
    );

    if (!newTree) {
        LOG_ERROR("[TreeSitter] Parse failed");
        return false;
    }

    if (tree_) ts_tree_delete(tree_);
    tree_ = newTree;
    lastSource_ = source;
    return true;
}

// Edit and reparse incrementally
// Duzenleme ve artimsal yeniden ayristirma
bool TreeSitterEngine::editAndReparse(
    int startLine, int startCol,
    int oldEndLine, int oldEndCol,
    int newEndLine, int newEndCol,
    const std::string& newSource) {

    std::lock_guard<std::mutex> lock(mutex_);

    if (!parser_ || !tree_) return false;

    // Calculate byte offsets from the old source
    // Eski kaynaktan bayt ofsetlerini hesapla
    auto calcOffset = [](const std::string& src, int line, int col) -> uint32_t {
        uint32_t offset = 0;
        int curLine = 0;
        for (size_t i = 0; i < src.size(); ++i) {
            if (curLine == line) {
                return offset + static_cast<uint32_t>(col);
            }
            if (src[i] == '\n') {
                ++curLine;
            }
            ++offset;
        }
        return offset + static_cast<uint32_t>(col);
    };

    uint32_t startByte = calcOffset(lastSource_, startLine, startCol);
    uint32_t oldEndByte = calcOffset(lastSource_, oldEndLine, oldEndCol);
    uint32_t newEndByte = calcOffset(newSource, newEndLine, newEndCol);

    TSInputEdit edit;
    edit.start_byte = startByte;
    edit.old_end_byte = oldEndByte;
    edit.new_end_byte = newEndByte;
    edit.start_point = {static_cast<uint32_t>(startLine), static_cast<uint32_t>(startCol)};
    edit.old_end_point = {static_cast<uint32_t>(oldEndLine), static_cast<uint32_t>(oldEndCol)};
    edit.new_end_point = {static_cast<uint32_t>(newEndLine), static_cast<uint32_t>(newEndCol)};

    ts_tree_edit(tree_, &edit);

    TSTree* newTree = ts_parser_parse_string(
        parser_,
        tree_,
        newSource.c_str(),
        static_cast<uint32_t>(newSource.size())
    );

    if (!newTree) {
        LOG_ERROR("[TreeSitter] Reparse failed");
        return false;
    }

    ts_tree_delete(tree_);
    tree_ = newTree;
    lastSource_ = newSource;
    return true;
}

// Check if tree exists
// Agacin var olup olmadigini kontrol et
bool TreeSitterEngine::hasTree() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tree_ != nullptr;
}

// Helper: convert TSNode to SyntaxNode (recursive with depth limit)
// Yardimci: TSNode'u SyntaxNode'a cevir (derinlik sinirlI ozyinelemeli)
SyntaxNode TreeSitterEngine::convertNode(void* nodePtr, int depth, int maxDepth) const {
    TSNode node = *static_cast<TSNode*>(nodePtr);
    SyntaxNode sn;

    const char* type = ts_node_type(node);
    sn.type = type ? type : "";
    sn.isNamed = ts_node_is_named(node);

    TSPoint start = ts_node_start_point(node);
    TSPoint end = ts_node_end_point(node);
    sn.startLine = static_cast<int>(start.row);
    sn.startCol  = static_cast<int>(start.column);
    sn.endLine   = static_cast<int>(end.row);
    sn.endCol    = static_cast<int>(end.column);

    if (depth < maxDepth) {
        uint32_t childCount = ts_node_child_count(node);
        sn.children.reserve(childCount);
        for (uint32_t i = 0; i < childCount; ++i) {
            TSNode child = ts_node_child(node, i);
            sn.children.push_back(convertNode(&child, depth + 1, maxDepth));

            // Set field name if available
            // Varsa alan adini ayarla
            const char* fieldName = ts_node_field_name_for_child(node, i);
            if (fieldName && !sn.children.empty()) {
                sn.children.back().fieldName = fieldName;
            }
        }
    }

    return sn;
}

// Get root node
// Kok dugumu al
SyntaxNode TreeSitterEngine::rootNode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!tree_) return {};

    TSNode root = ts_tree_root_node(tree_);
    return convertNode(const_cast<void*>(static_cast<const void*>(&root)), 0, 3);
}

// Get node at position
// Konumdaki dugumu al
SyntaxNode TreeSitterEngine::nodeAt(int line, int col) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!tree_) return {};

    TSNode root = ts_tree_root_node(tree_);
    TSPoint point = {static_cast<uint32_t>(line), static_cast<uint32_t>(col)};
    TSNode node = ts_node_descendant_for_point_range(root, point, point);

    return convertNode(const_cast<void*>(static_cast<const void*>(&node)), 0, 1);
}

// Get named node at position
// Konumdaki adlandirilmis dugumu al
SyntaxNode TreeSitterEngine::namedNodeAt(int line, int col) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!tree_) return {};

    TSNode root = ts_tree_root_node(tree_);
    TSPoint point = {static_cast<uint32_t>(line), static_cast<uint32_t>(col)};
    TSNode node = ts_node_named_descendant_for_point_range(root, point, point);

    return convertNode(const_cast<void*>(static_cast<const void*>(&node)), 0, 1);
}

// Run query on tree
// Agac uzerinde sorgu calistir
std::vector<QueryMatch> TreeSitterEngine::query(
    const std::string& queryStr, const std::string& source,
    int startLine, int endLine) const {

    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<QueryMatch> results;

    if (!tree_ || !parser_) return results;

    const TSLanguage* lang = ts_parser_language(parser_);
    if (!lang) return results;

    uint32_t errorOffset = 0;
    TSQueryError errorType = TSQueryErrorNone;
    TSQuery* tsQuery = ts_query_new(
        lang,
        queryStr.c_str(),
        static_cast<uint32_t>(queryStr.size()),
        &errorOffset,
        &errorType
    );

    if (!tsQuery) {
        LOG_ERROR("[TreeSitter] Query error at offset ", errorOffset, ": type=", static_cast<int>(errorType));
        return results;
    }

    TSQueryCursor* cursor = ts_query_cursor_new();
    TSNode root = ts_tree_root_node(tree_);
    ts_query_cursor_exec(cursor, tsQuery, root);

    // Set row range if specified
    // Belirtildiyse satir araligini ayarla
    if (endLine >= 0) {
        ts_query_cursor_set_point_range(
            cursor,
            TSPoint{static_cast<uint32_t>(startLine), 0},
            TSPoint{static_cast<uint32_t>(endLine + 1), 0}
        );
    }

    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {
        QueryMatch qm;
        qm.patternIndex = static_cast<int>(match.pattern_index);

        for (uint16_t i = 0; i < match.capture_count; ++i) {
            TSQueryCapture cap = match.captures[i];

            uint32_t nameLen = 0;
            const char* name = ts_query_capture_name_for_id(tsQuery, cap.index, &nameLen);

            TSPoint cStart = ts_node_start_point(cap.node);
            TSPoint cEnd   = ts_node_end_point(cap.node);

            uint32_t startByte = ts_node_start_byte(cap.node);
            uint32_t endByte   = ts_node_end_byte(cap.node);

            QueryCapture qc;
            qc.name = name ? std::string(name, nameLen) : "";
            qc.startLine = static_cast<int>(cStart.row);
            qc.startCol  = static_cast<int>(cStart.column);
            qc.endLine   = static_cast<int>(cEnd.row);
            qc.endCol    = static_cast<int>(cEnd.column);

            // Extract captured text from source
            // Kaynaktan yakalanan metni cikar
            if (startByte < source.size() && endByte <= source.size()) {
                qc.text = source.substr(startByte, endByte - startByte);
            }

            qm.captures.push_back(std::move(qc));
        }

        results.push_back(std::move(qm));
    }

    ts_query_cursor_delete(cursor);
    ts_query_delete(tsQuery);

    return results;
}

// Helper: collect error nodes recursively
// Yardimci: hata dugumlerini ozyinelemeli olarak topla
void TreeSitterEngine::collectErrors(void* nodePtr, std::vector<SyntaxNode>& errors) const {
    TSNode node = *static_cast<TSNode*>(nodePtr);

    if (ts_node_is_error(node) || ts_node_is_missing(node)) {
        errors.push_back(convertNode(nodePtr, 0, 0));
    }

    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; ++i) {
        TSNode child = ts_node_child(node, i);
        collectErrors(&child, errors);
    }
}

// Get all syntax errors
// Tum soz dizimi hatalarini al
std::vector<SyntaxNode> TreeSitterEngine::errors() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SyntaxNode> errs;
    if (!tree_) return errs;

    TSNode root = ts_tree_root_node(tree_);
    collectErrors(&root, errs);
    return errs;
}

// Reset (free tree)
// Sifirla (agaci serbest birak)
void TreeSitterEngine::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tree_) {
        ts_tree_delete(tree_);
        tree_ = nullptr;
    }
    lastSource_.clear();
}

#endif // BERKIDE_TREESITTER_ENABLED
