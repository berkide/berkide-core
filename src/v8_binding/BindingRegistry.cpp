// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "BindingRegistry.h"
#include "EditorContext.h"
#include <algorithm>

// =============================================================================
// Binding manifest: her binding burada listelenir.
// Yeni binding eklerken buraya bir satir eklemen yeterli.
// =============================================================================
REGISTER_BINDING(AutoSave)
REGISTER_BINDING(Buffer)
REGISTER_BINDING(BufferOptions)
REGISTER_BINDING(Buffers)
REGISTER_BINDING(CharClassifier)
REGISTER_BINDING(Commands)
REGISTER_BINDING(Completion)
REGISTER_BINDING(Config)
REGISTER_BINDING(Cursor)
REGISTER_BINDING(Diff)
REGISTER_BINDING(Encoding)
REGISTER_BINDING(Event)
REGISTER_BINDING(Extmark)
REGISTER_BINDING(File)
REGISTER_BINDING(Fold)
REGISTER_BINDING(Help)
REGISTER_BINDING(HttpServer)
REGISTER_BINDING(I18n)
REGISTER_BINDING(Indent)
REGISTER_BINDING(Input)
REGISTER_BINDING(Keymap)
REGISTER_BINDING(Macro)
REGISTER_BINDING(Mark)
REGISTER_BINDING(MultiCursor)
REGISTER_BINDING(Plugin)
REGISTER_BINDING(Process)
REGISTER_BINDING(Register)
REGISTER_BINDING(Search)
REGISTER_BINDING(Selection)
REGISTER_BINDING(Session)
REGISTER_BINDING(State)
#ifdef BERKIDE_TREESITTER_ENABLED
REGISTER_BINDING(TreeSitter)
#endif
REGISTER_BINDING(Undo)
REGISTER_BINDING(Wasm)
REGISTER_BINDING(WebSocket)
REGISTER_BINDING(Window)
REGISTER_BINDING(Worker)

// Singleton accessor for the global binding registry
// Global binding kayit defterine erisim icin singleton erisimci
BindingRegistry& BindingRegistry::instance() {
    static BindingRegistry reg;
    return reg;
}

// Register a named binding function; preserves insertion order for deterministic apply
// Isimli bir binding fonksiyonu kaydet; belirli sirada uygulamak icin ekleme sirasini koru
void BindingRegistry::registerBinding(const std::string& name, BindingRegisterFunc func) {
    const bool exists = map_.find(name) != map_.end();
    map_[name] = std::move(func);
    if (!exists) order_.push_back(name);
}

// Remove a binding by name from both map and ordered list
// Binding'i hem map'ten hem de sirali listeden ismine gore kaldir
void BindingRegistry::removeBinding(const std::string& name) {
    map_.erase(name);
    order_.erase(std::remove(order_.begin(), order_.end(), name), order_.end());
}

// Apply all registered bindings to the editor JS object in registration order
// Tum kayitli binding'leri kayit sirasina gore editor JS nesnesine uygula
void BindingRegistry::applyAll(v8::Isolate* isolate, v8::Local<v8::Object> jsEditor, EditorContext& ctx) {
    for (const auto& name : order_) {
        auto it = map_.find(name);
        if (it != map_.end()) {
            it->second(isolate, jsEditor, ctx);
        }
    }
}

// Apply a single named binding to the editor JS object; returns false if not found
// Tek bir isimli binding'i editor JS nesnesine uygula; bulunamazsa false dondur
bool BindingRegistry::applyOne(const std::string& name, v8::Isolate* isolate, v8::Local<v8::Object> jsEditor, EditorContext& ctx) {
    auto it = map_.find(name);
    if (it == map_.end()) return false;
    it->second(isolate, jsEditor, ctx);
    return true;
}

// Return the list of all registered binding names in order
// Tum kayitli binding isimlerini sirali olarak dondur
std::vector<std::string> BindingRegistry::list() const {
    return order_;
}
