#include "EncodingBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "EncodingDetector.h"

// Register editor.encoding JS API
// editor.encoding JS API'sini kaydet
void RegisterEncodingBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsEnc = v8::Object::New(isolate);

    // editor.encoding.detectFile(path) -> {encoding, hasBOM, bomSize, confidence}
    // Dosya kodlamasini algila
    jsEnc->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "detectFile"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            if (args.Length() < 1 || !args[0]->IsString()) {
                args.GetReturnValue().SetNull();
                return;
            }

            v8::String::Utf8Value path(iso, args[0]);
            EncodingResult r = EncodingDetector::detectFile(*path);

            v8::Local<v8::Object> result = v8::Object::New(iso);
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "encoding"),
                        v8::String::NewFromUtf8(iso, EncodingDetector::encodingName(r.encoding).c_str()).ToLocalChecked()).Check();
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "hasBOM"),
                        v8::Boolean::New(iso, r.hasBOM)).Check();
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "bomSize"),
                        v8::Integer::New(iso, r.bomSize)).Check();
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "confidence"),
                        v8::Number::New(iso, r.confidence)).Check();

            args.GetReturnValue().Set(result);
        }).ToLocalChecked()
    ).Check();

    // editor.encoding.detect(arrayBuffer) -> {encoding, hasBOM, bomSize, confidence}
    // Bayt dizisinin kodlamasini algila
    jsEnc->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "detect"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            if (args.Length() < 1) { args.GetReturnValue().SetNull(); return; }

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
                args.GetReturnValue().SetNull();
                return;
            }

            EncodingResult r = EncodingDetector::detect(data);

            v8::Local<v8::Object> result = v8::Object::New(iso);
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "encoding"),
                        v8::String::NewFromUtf8(iso, EncodingDetector::encodingName(r.encoding).c_str()).ToLocalChecked()).Check();
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "hasBOM"),
                        v8::Boolean::New(iso, r.hasBOM)).Check();
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "bomSize"),
                        v8::Integer::New(iso, r.bomSize)).Check();
            result->Set(ctx, v8::String::NewFromUtf8Literal(iso, "confidence"),
                        v8::Number::New(iso, r.confidence)).Check();
            args.GetReturnValue().Set(result);
        }).ToLocalChecked()
    ).Check();

    // editor.encoding.toUTF8(arrayBuffer, encoding) -> string
    // Baytlari UTF-8'e donustur
    jsEnc->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "toUTF8"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();

            if (args.Length() < 2) { args.GetReturnValue().SetUndefined(); return; }

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
                args.GetReturnValue().SetUndefined();
                return;
            }

            v8::String::Utf8Value encName(iso, args[1]);
            TextEncoding enc = EncodingDetector::parseEncoding(*encName);

            std::string result = EncodingDetector::toUTF8(data, enc);
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(iso, result.c_str(),
                                        v8::NewStringType::kNormal,
                                        static_cast<int>(result.size())).ToLocalChecked());
        }).ToLocalChecked()
    ).Check();

    // editor.encoding.isValidUTF8(string) -> bool
    // Gecerli UTF-8 mi kontrol et
    jsEnc->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isValidUTF8"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            if (args.Length() < 1 || !args[0]->IsString()) {
                args.GetReturnValue().Set(false);
                return;
            }
            v8::String::Utf8Value str(iso, args[0]);
            bool valid = EncodingDetector::isValidUTF8(
                reinterpret_cast<const uint8_t*>(*str),
                static_cast<size_t>(str.length()));
            args.GetReturnValue().Set(valid);
        }).ToLocalChecked()
    ).Check();

    // editor.encoding.name(encodingStr) -> string (normalized name)
    // Normalize edilmis kodlama adini dondur
    jsEnc->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "name"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            if (args.Length() < 1 || !args[0]->IsString()) {
                args.GetReturnValue().Set(v8::String::NewFromUtf8Literal(iso, "unknown"));
                return;
            }
            v8::String::Utf8Value name(iso, args[0]);
            TextEncoding enc = EncodingDetector::parseEncoding(*name);
            std::string normalized = EncodingDetector::encodingName(enc);
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(iso, normalized.c_str()).ToLocalChecked());
        }).ToLocalChecked()
    ).Check();

    // editor.encoding.fromUTF8(text, encoding) -> ArrayBuffer (encoded bytes)
    // UTF-8 dizesini hedef kodlamaya donustur
    jsEnc->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "fromUTF8"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();

            if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
                args.GetReturnValue().SetUndefined();
                return;
            }

            v8::String::Utf8Value utf8Text(iso, args[0]);
            v8::String::Utf8Value encName(iso, args[1]);

            std::string text = *utf8Text ? *utf8Text : "";
            TextEncoding enc = EncodingDetector::parseEncoding(*encName);

            std::vector<uint8_t> result = EncodingDetector::fromUTF8(text, enc);

            // Return as ArrayBuffer
            // ArrayBuffer olarak dondur
            auto backingStore = v8::ArrayBuffer::NewBackingStore(iso, result.size());
            if (result.size() > 0) {
                memcpy(backingStore->Data(), result.data(), result.size());
            }
            auto arrayBuffer = v8::ArrayBuffer::New(iso, std::move(backingStore));
            args.GetReturnValue().Set(arrayBuffer);
        }).ToLocalChecked()
    ).Check();

    // editor.encoding.isASCII(text) -> bool - Check if text is pure 7-bit ASCII
    // Metnin saf 7-bit ASCII olup olmadigini kontrol et
    jsEnc->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "isASCII"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto iso = args.GetIsolate();
            if (args.Length() < 1 || !args[0]->IsString()) {
                args.GetReturnValue().Set(false);
                return;
            }
            v8::String::Utf8Value str(iso, args[0]);
            bool ascii = EncodingDetector::isASCII(
                reinterpret_cast<const uint8_t*>(*str),
                static_cast<size_t>(str.length()));
            args.GetReturnValue().Set(ascii);
        }).ToLocalChecked()
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
