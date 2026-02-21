#include "BufferOptionsBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "BufferOptions.h"
#include <v8.h>

// Helper: Convert JS value to OptionValue variant based on detected type
// Yardimci: JS degerini algilanan tipe gore OptionValue variant'ina donustur
static OptionValue JsToOptionValue(v8::Isolate* iso, v8::Local<v8::Value> val) {
    if (val->IsBoolean()) {
        return val->BooleanValue(iso);
    }
    if (val->IsInt32()) {
        return val->Int32Value(iso->GetCurrentContext()).FromJust();
    }
    if (val->IsNumber()) {
        return val->NumberValue(iso->GetCurrentContext()).FromJust();
    }
    // Default to string
    // Varsayilan olarak string'e donustur
    v8::String::Utf8Value str(iso, val);
    return std::string(*str ? *str : "");
}

// Helper: Convert OptionValue variant to JS value
// Yardimci: OptionValue variant'ini JS degerine donustur
static v8::Local<v8::Value> OptionValueToJs(v8::Isolate* iso, const OptionValue& ov) {
    return std::visit([iso](auto&& arg) -> v8::Local<v8::Value> {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>) {
            return v8::Integer::New(iso, arg);
        } else if constexpr (std::is_same_v<T, bool>) {
            return v8::Boolean::New(iso, arg);
        } else if constexpr (std::is_same_v<T, double>) {
            return v8::Number::New(iso, arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return v8::String::NewFromUtf8(iso, arg.c_str()).ToLocalChecked();
        } else {
            return v8::Undefined(iso);
        }
    }, ov);
}

// Register editor.options JS object with all BufferOptions methods
// editor.options JS nesnesini tum BufferOptions metodlariyla kaydet
void RegisterBufferOptionsBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsOpts = v8::Object::New(isolate);

    auto* opts = edCtx.bufferOptions;
    v8::Local<v8::External> extData = v8::External::New(isolate, opts);

    // options.setDefault(key, value) - Set global default option
    // Global varsayilan secenegi ayarla
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setDefault"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* o = static_cast<BufferOptions*>(args.Data().As<v8::External>()->Value());
            if (!o || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            v8::String::Utf8Value key(iso, args[0]);
            OptionValue val = JsToOptionValue(iso, args[1]);
            o->setDefault(*key ? *key : "", val);
        }, extData).ToLocalChecked()
    ).Check();

    // options.getDefault(key) -> value - Get global default option
    // Global varsayilan secenegi al
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getDefault"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* o = static_cast<BufferOptions*>(args.Data().As<v8::External>()->Value());
            if (!o || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            v8::String::Utf8Value key(iso, args[0]);
            auto result = o->getDefault(*key ? *key : "");
            if (result.has_value()) {
                args.GetReturnValue().Set(OptionValueToJs(iso, *result));
            } else {
                args.GetReturnValue().Set(v8::Undefined(iso));
            }
        }, extData).ToLocalChecked()
    ).Check();

    // options.setLocal(bufferId, key, value) - Set buffer-local option
    // Buffer-yerel secenegi ayarla
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "setLocal"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* o = static_cast<BufferOptions*>(args.Data().As<v8::External>()->Value());
            if (!o || args.Length() < 3) return;
            auto* iso = args.GetIsolate();
            int bufferId = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            v8::String::Utf8Value key(iso, args[1]);
            OptionValue val = JsToOptionValue(iso, args[2]);
            o->setLocal(bufferId, *key ? *key : "", val);
        }, extData).ToLocalChecked()
    ).Check();

    // options.removeLocal(bufferId, key) - Remove buffer-local override
    // Buffer-yerel gecersiz kilmayi kaldir
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "removeLocal"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* o = static_cast<BufferOptions*>(args.Data().As<v8::External>()->Value());
            if (!o || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            int bufferId = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            v8::String::Utf8Value key(iso, args[1]);
            o->removeLocal(bufferId, *key ? *key : "");
        }, extData).ToLocalChecked()
    ).Check();

    // options.get(bufferId, key) -> value - Get effective option (local > default)
    // Gecerli secenegi al (yerel > varsayilan)
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "get"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* o = static_cast<BufferOptions*>(args.Data().As<v8::External>()->Value());
            if (!o || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            int bufferId = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            v8::String::Utf8Value key(iso, args[1]);
            auto result = o->get(bufferId, *key ? *key : "");
            if (result.has_value()) {
                args.GetReturnValue().Set(OptionValueToJs(iso, *result));
            } else {
                args.GetReturnValue().Set(v8::Undefined(iso));
            }
        }, extData).ToLocalChecked()
    ).Check();

    // options.hasLocal(bufferId, key) -> bool - Check if buffer has local override
    // Buffer'in yerel gecersiz kilmasi olup olmadigini kontrol et
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "hasLocal"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* o = static_cast<BufferOptions*>(args.Data().As<v8::External>()->Value());
            if (!o || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            int bufferId = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            v8::String::Utf8Value key(iso, args[1]);
            bool has = o->hasLocal(bufferId, *key ? *key : "");
            args.GetReturnValue().Set(v8::Boolean::New(iso, has));
        }, extData).ToLocalChecked()
    ).Check();

    // options.listKeys(bufferId) -> string[] - List all option keys for buffer
    // Bir buffer icin tum secenek anahtarlarini listele
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listKeys"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* o = static_cast<BufferOptions*>(args.Data().As<v8::External>()->Value());
            if (!o || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            int bufferId = args[0]->Int32Value(ctx).FromJust();
            auto keys = o->listKeys(bufferId);
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(keys.size()));
            for (size_t i = 0; i < keys.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i),
                    v8::String::NewFromUtf8(iso, keys[i].c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(arr);
        }, extData).ToLocalChecked()
    ).Check();

    // options.listLocalKeys(bufferId) -> string[] - List buffer-local override keys
    // Buffer-yerel gecersiz kilma anahtarlarini listele
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listLocalKeys"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* o = static_cast<BufferOptions*>(args.Data().As<v8::External>()->Value());
            if (!o || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            int bufferId = args[0]->Int32Value(ctx).FromJust();
            auto keys = o->listLocalKeys(bufferId);
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(keys.size()));
            for (size_t i = 0; i < keys.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i),
                    v8::String::NewFromUtf8(iso, keys[i].c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(arr);
        }, extData).ToLocalChecked()
    ).Check();

    // options.listDefaultKeys() -> string[] - List all global default keys
    // Tum global varsayilan anahtarlari listele
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "listDefaultKeys"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* o = static_cast<BufferOptions*>(args.Data().As<v8::External>()->Value());
            if (!o) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            auto keys = o->listDefaultKeys();
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(keys.size()));
            for (size_t i = 0; i < keys.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i),
                    v8::String::NewFromUtf8(iso, keys[i].c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(arr);
        }, extData).ToLocalChecked()
    ).Check();

    // options.clearBuffer(bufferId) - Clear all local options for a buffer
    // Bir buffer icin tum yerel secenekleri temizle
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "clearBuffer"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* o = static_cast<BufferOptions*>(args.Data().As<v8::External>()->Value());
            if (!o || args.Length() < 1) return;
            auto* iso = args.GetIsolate();
            int bufferId = args[0]->Int32Value(iso->GetCurrentContext()).FromJust();
            o->clearBuffer(bufferId);
        }, extData).ToLocalChecked()
    ).Check();

    // options.getInt(bufferId, key, fallback) -> int - Get option as integer
    // Secenegi tam sayi olarak al
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getInt"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* o = static_cast<BufferOptions*>(args.Data().As<v8::External>()->Value());
            if (!o || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            int bufferId = args[0]->Int32Value(ctx).FromJust();
            v8::String::Utf8Value key(iso, args[1]);
            int fallback = 0;
            if (args.Length() >= 3) {
                fallback = args[2]->Int32Value(ctx).FromJust();
            }
            int result = o->getInt(bufferId, *key ? *key : "", fallback);
            args.GetReturnValue().Set(v8::Integer::New(iso, result));
        }, extData).ToLocalChecked()
    ).Check();

    // options.getBool(bufferId, key, fallback) -> bool - Get option as boolean
    // Secenegi mantiksal deger olarak al
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getBool"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* o = static_cast<BufferOptions*>(args.Data().As<v8::External>()->Value());
            if (!o || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            int bufferId = args[0]->Int32Value(ctx).FromJust();
            v8::String::Utf8Value key(iso, args[1]);
            bool fallback = false;
            if (args.Length() >= 3) {
                fallback = args[2]->BooleanValue(iso);
            }
            bool result = o->getBool(bufferId, *key ? *key : "", fallback);
            args.GetReturnValue().Set(v8::Boolean::New(iso, result));
        }, extData).ToLocalChecked()
    ).Check();

    // options.getString(bufferId, key, fallback) -> string - Get option as string
    // Secenegi metin olarak al
    jsOpts->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "getString"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* o = static_cast<BufferOptions*>(args.Data().As<v8::External>()->Value());
            if (!o || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();
            int bufferId = args[0]->Int32Value(ctx).FromJust();
            v8::String::Utf8Value key(iso, args[1]);
            std::string fallback = "";
            if (args.Length() >= 3) {
                v8::String::Utf8Value fb(iso, args[2]);
                fallback = *fb ? *fb : "";
            }
            std::string result = o->getString(bufferId, *key ? *key : "", fallback);
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(iso, result.c_str()).ToLocalChecked());
        }, extData).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "options"),
        jsOpts).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _reg = [] {
    BindingRegistry::instance().registerBinding("options", RegisterBufferOptionsBinding);
    return true;
}();
