// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "EncodingBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
#include "EncodingDetector.h"

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Context for encoding binding
// Kodlama binding baglami
struct EncodingCtx {
    I18n* i18n;
};

// Register editor.encoding JS API with standard response format
// Standart yanit formatiyla editor.encoding JS API'sini kaydet
void RegisterEncodingBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsEnc = v8::Object::New(isolate);

    auto* ectx = new EncodingCtx{ctx.i18n};

    // editor.encoding.detectFile(path) -> {ok, data: {encoding, hasBOM, bomSize, confidence}}
    // Dosya kodlamasini algila
    jsEnc->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "detectFile"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<EncodingCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 1 || !args[0]->IsString()) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "path"}}, ec->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            std::string path = v8Str(iso, args[0]);
            EncodingResult r = EncodingDetector::detectFile(path.c_str());

            json data = {
                {"encoding", EncodingDetector::encodingName(r.encoding)},
                {"hasBOM", r.hasBOM},
                {"bomSize", r.bomSize},
                {"confidence", r.confidence}
            };
            V8Response::ok(args, data);
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
    ).Check();

    // editor.encoding.detect(arrayBuffer) -> {ok, data: {encoding, hasBOM, bomSize, confidence}}
    // Bayt dizisinin kodlamasini algila
    jsEnc->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "detect"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<EncodingCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 1) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "data"}}, ec->i18n);
                return;
            }
            auto iso = args.GetIsolate();

            std::vector<uint8_t> data;
            if (args[0]->IsString()) {
                v8::String::Utf8Value str(iso, args[0]);
                data.assign(reinterpret_cast<const uint8_t*>(*str),
                           reinterpret_cast<const uint8_t*>(*str) + str.length());
            } else if (args[0]->IsArrayBufferView()) {
                auto view = args[0].As<v8::ArrayBufferView>();
                size_t len = view->ByteLength();
                data.resize(len);
                view->CopyContents(data.data(), len);
            } else {
                V8Response::error(args, "INVALID_ARG", "args.invalid_type",
                    {{"name", "data"}, {"expected", "string or ArrayBufferView"}}, ec->i18n);
                return;
            }

            EncodingResult r = EncodingDetector::detect(data);

            json result = {
                {"encoding", EncodingDetector::encodingName(r.encoding)},
                {"hasBOM", r.hasBOM},
                {"bomSize", r.bomSize},
                {"confidence", r.confidence}
            };
            V8Response::ok(args, result);
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
    ).Check();

    // editor.encoding.toUTF8(arrayBuffer, encoding) -> {ok, data: string}
    // Baytlari UTF-8'e donustur
    jsEnc->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "toUTF8"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<EncodingCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "data, encoding"}}, ec->i18n);
                return;
            }
            auto iso = args.GetIsolate();

            std::vector<uint8_t> data;
            if (args[0]->IsArrayBufferView()) {
                auto view = args[0].As<v8::ArrayBufferView>();
                size_t len = view->ByteLength();
                data.resize(len);
                view->CopyContents(data.data(), len);
            } else if (args[0]->IsString()) {
                v8::String::Utf8Value str(iso, args[0]);
                data.assign(reinterpret_cast<const uint8_t*>(*str),
                           reinterpret_cast<const uint8_t*>(*str) + str.length());
            } else {
                V8Response::error(args, "INVALID_ARG", "args.invalid_type",
                    {{"name", "data"}, {"expected", "string or ArrayBufferView"}}, ec->i18n);
                return;
            }

            std::string encName = v8Str(iso, args[1]);
            TextEncoding enc = EncodingDetector::parseEncoding(encName.c_str());

            std::string result = EncodingDetector::toUTF8(data, enc);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
    ).Check();

    // editor.encoding.isValidUTF8(string) -> {ok, data: bool}
    // Gecerli UTF-8 mi kontrol et
    jsEnc->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isValidUTF8"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<EncodingCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 1 || !args[0]->IsString()) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "text"}}, ec->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            v8::String::Utf8Value str(iso, args[0]);
            bool valid = EncodingDetector::isValidUTF8(
                reinterpret_cast<const uint8_t*>(*str),
                static_cast<size_t>(str.length()));
            V8Response::ok(args, valid);
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
    ).Check();

    // editor.encoding.name(encodingStr) -> {ok, data: string}
    // Normalize edilmis kodlama adini dondur
    jsEnc->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "name"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<EncodingCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 1 || !args[0]->IsString()) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "encodingStr"}}, ec->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            std::string name = v8Str(iso, args[0]);
            TextEncoding enc = EncodingDetector::parseEncoding(name.c_str());
            std::string normalized = EncodingDetector::encodingName(enc);
            V8Response::ok(args, normalized);
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
    ).Check();

    // editor.encoding.fromUTF8(text, encoding) -> {ok, data: ArrayBuffer}
    // UTF-8 dizesini hedef kodlamaya donustur
    // NOTE: This method returns raw ArrayBuffer via V8, not wrapped in standard response
    // NOT: Bu metod ham ArrayBuffer dondurur, standart yanit formatinda degildir
    jsEnc->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "fromUTF8"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<EncodingCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "text, encoding"}}, ec->i18n);
                return;
            }
            auto iso = args.GetIsolate();

            std::string text = v8Str(iso, args[0]);
            std::string encName = v8Str(iso, args[1]);
            TextEncoding enc = EncodingDetector::parseEncoding(encName.c_str());

            std::vector<uint8_t> result = EncodingDetector::fromUTF8(text, enc);

            // Return as ArrayBuffer (binary data cannot be JSON-wrapped)
            // ArrayBuffer olarak dondur (ikili veri JSON'a sarilamaz)
            auto backingStore = v8::ArrayBuffer::NewBackingStore(iso, result.size());
            if (result.size() > 0) {
                memcpy(backingStore->Data(), result.data(), result.size());
            }
            auto arrayBuffer = v8::ArrayBuffer::New(iso, std::move(backingStore));
            args.GetReturnValue().Set(arrayBuffer);
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
    ).Check();

    // editor.encoding.isASCII(text) -> {ok, data: bool}
    // Metnin saf 7-bit ASCII olup olmadigini kontrol et
    jsEnc->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isASCII"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* ec = static_cast<EncodingCtx*>(args.Data().As<v8::External>()->Value());
            if (!ec) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_context", {}, nullptr);
                return;
            }
            if (args.Length() < 1 || !args[0]->IsString()) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "text"}}, ec->i18n);
                return;
            }
            auto iso = args.GetIsolate();
            v8::String::Utf8Value str(iso, args[0]);
            bool ascii = EncodingDetector::isASCII(
                reinterpret_cast<const uint8_t*>(*str),
                static_cast<size_t>(str.length()));
            V8Response::ok(args, ascii);
        }, v8::External::New(isolate, ectx)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "encoding"), jsEnc).Check();
}

// Auto-register binding
// Binding'i otomatik kaydet
static bool _encodingReg = [] {
    BindingRegistry::instance().registerBinding("encoding", RegisterEncodingBinding);
    return true;
}();
