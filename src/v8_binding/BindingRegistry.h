// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <v8.h>

struct EditorContext;

// Function signature for binding registration: (isolate, editorObject, context)
// Binding kayit fonksiyon imzasi: (izolasyon, editorNesnesi, baglam)
using BindingRegisterFunc = std::function<void(v8::Isolate*, v8::Local<v8::Object>, EditorContext&)>;

// Central registry for all V8 bindings (buffer, cursor, events, etc.).
// Tum V8 binding'leri icin merkezi kayit defteri (buffer, cursor, events, vb.).
// Each binding self-registers at static initialization time.
// Her binding statik baslatma zamaninda kendini kaydeder.
class BindingRegistry {
public:
    // Singleton access
    // Tekil erisim
    static BindingRegistry& instance();

    // Register a new binding with a unique name
    // Benzersiz bir isimle yeni bir binding kaydet
    void registerBinding(const std::string& name, BindingRegisterFunc func);

    // Remove a binding by name
    // Ismiyle bir binding'i kaldir
    void removeBinding(const std::string& name);

    // Apply all registered bindings to an editor object
    // Tum kayitli binding'leri bir editor nesnesine uygula
    void applyAll(v8::Isolate* isolate, v8::Local<v8::Object> jsEditor, EditorContext& ctx);

    // Apply a single binding by name
    // Ismiyle tek bir binding uygula
    bool applyOne(const std::string& name, v8::Isolate* isolate, v8::Local<v8::Object> jsEditor, EditorContext& ctx);

    // List all registered binding names
    // Tum kayitli binding isimlerini listele
    std::vector<std::string> list() const;

private:
    std::unordered_map<std::string, BindingRegisterFunc> map_;   // Name -> function map / Isim -> fonksiyon haritasi
    std::vector<std::string> order_;                              // Registration order / Kayit sirasi
};

// =============================================================================
// REGISTER_BINDING macro — ensures the linker includes the binding's .o file.
// Yeni binding eklemek icin:
//   1. XxxBinding.h / XxxBinding.cpp olustur (RegisterXxxBinding fonksiyonu ile)
//   2. BindingRegistry.cpp'ye REGISTER_BINDING(Xxx) ekle
// =============================================================================
#define REGISTER_BINDING(Name) \
    extern void Register##Name##Binding(v8::Isolate*, v8::Local<v8::Object>, EditorContext&); \
    __attribute__((used)) static void* _force_link_##Name = (void*)&Register##Name##Binding;
