

## core ve binding geliştirme rehberi 



🧩 Yeni Core Modülü ve V8 Binding Yazım Rehberi

1️⃣ Ön Bilgi

BerkIDE çekirdeği C++ core + V8 binding yapısından oluşur.
Core tarafı sistemin saf C++ mantığını içerir;
V8 tarafı ise bu mantığı JavaScript (editor.*) üzerinden erişilebilir hale getirir.

Yani:
	•	customcore.cpp/.h → C++ mantığı
	•	CustomCoreBinding.cpp/.h → JS tarafına köprü

⸻

2️⃣ Yeni Bir Core Modülü Oluşturmak

Örneğin yeni bir modül yazdınız:


// customcore.h
#pragma once
#include <string>
#include <iostream>

class CustomCore {
public:
    // 1️⃣ Parametre almayan, dönüşü olmayan fonksiyon
    void sayHello();

    // 2️⃣ Parametre alan, dönüşü olmayan fonksiyon
    void printMessage(const std::string& msg);

    // 3️⃣ Parametre almayan, dönüş değeri olan fonksiyon
    int getRandomNumber();

    // 4️⃣ Parametre alan, dönüş değeri olan fonksiyon
    std::string repeatText(const std::string& text, int count);
};


// customcore.cpp
#include "customcore.h"
#include <cstdlib>

void CustomCore::sayHello() {
    std::cout << "Merhaba CustomCore!" << std::endl;
}

void CustomCore::printMessage(const std::string& msg) {
    std::cout << "[JS Message]: " << msg << std::endl;
}

int CustomCore::getRandomNumber() {
    return std::rand() % 100;
}

std::string CustomCore::repeatText(const std::string& text, int count) {
    std::string result;
    for (int i = 0; i < count; ++i)
        result += text;
    return result;
}


V8 Binding Dosyası

Binding, core fonksiyonlarını JavaScript tarafına aktarır.



// CustomCoreBinding.h
#pragma once
#include <v8.h>

class CustomCore;

// 📘 CustomCoreBinding
// CustomCore’u JS tarafına bağlar.
// editor.customcore objesini oluşturur.
void RegisterCustomCoreBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, CustomCore* core);




// CustomCoreBinding.cpp
#include "CustomCoreBinding.h"
#include "BindingRegistry.h"
#include "customcore.h"
#include <v8.h>

// Tek global örnek
static CustomCore g_core;

void RegisterCustomCoreBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, CustomCore* core) {
    auto ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsCore = v8::Object::New(isolate);

    // === core.sayHello()
    jsCore->Set(ctx,
        v8::String::NewFromUtf8Literal(isolate, "sayHello"),
        v8::Function::New(ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<CustomCore*>(args.Data().As<v8::External>()->Value());
            c->sayHello();
        }, v8::External::New(isolate, core)).ToLocalChecked()
    ).Check();

    // === core.printMessage(msg)
    jsCore->Set(ctx,
        v8::String::NewFromUtf8Literal(isolate, "printMessage"),
        v8::Function::New(ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 1) return;
            v8::String::Utf8Value msg(args.GetIsolate(), args[0]);
            auto* c = static_cast<CustomCore*>(args.Data().As<v8::External>()->Value());
            c->printMessage(*msg);
        }, v8::External::New(isolate, core)).ToLocalChecked()
    ).Check();

    // === core.getRandomNumber()
    jsCore->Set(ctx,
        v8::String::NewFromUtf8Literal(isolate, "getRandomNumber"),
        v8::Function::New(ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* c = static_cast<CustomCore*>(args.Data().As<v8::External>()->Value());
            int num = c->getRandomNumber();
            args.GetReturnValue().Set(v8::Integer::New(args.GetIsolate(), num));
        }, v8::External::New(isolate, core)).ToLocalChecked()
    ).Check();

    // === core.repeatText(text, count)
    jsCore->Set(ctx,
        v8::String::NewFromUtf8Literal(isolate, "repeatText"),
        v8::Function::New(ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            if (args.Length() < 2) return;
            v8::String::Utf8Value text(args.GetIsolate(), args[0]);
            int count = args[1]->Int32Value(args.GetIsolate()->GetCurrentContext()).FromMaybe(1);
            auto* c = static_cast<CustomCore*>(args.Data().As<v8::External>()->Value());
            std::string result = c->repeatText(*text, count);
            args.GetReturnValue().Set(v8::String::NewFromUtf8(args.GetIsolate(), result.c_str()).ToLocalChecked());
        }, v8::External::New(isolate, core)).ToLocalChecked()
    ).Check();

    // JS tarafında editor.customcore olarak kaydet
    editorObj->Set(ctx, v8::String::NewFromUtf8Literal(isolate, "customcore"), jsCore).Check();
}

// 🔧 Otomatik kayıt
static bool registered_customcore = []{
    BindingRegistry::instance().registerBinding("customcore",
        [](v8::Isolate* isolate, v8::Local<v8::Object> editorObj){
            RegisterCustomCoreBinding(isolate, editorObj, &g_core);
        });
    return true;
}();


artık  JS Tarafında Kullanımı

editor.customcore.sayHello();                     // Konsola "Merhaba CustomCore!" yazar
editor.customcore.printMessage("Selam V8!");      // C++ tarafına mesaj gönderir
let num = editor.customcore.getRandomNumber();    // JS tarafında rastgele sayı döner
let txt = editor.customcore.repeatText("ha", 3);  // "hahaha" döner


şeklinde olucaktır 




Tür
C++ Örneği
JS Çağrısı
Dönüş
Düz fonksiyon
void sayHello()
editor.customcore.sayHello()
Yok
Parametreli (dönüşsüz)
void printMessage(std::string)
editor.customcore.printMessage("Hi")
Yok
Dönüşlü (parametresiz)
int getRandomNumber()
let n = editor.customcore.getRandomNumber()
Sayı
Parametre + dönüş
std::string repeatText(std::string,int)
editor.customcore.repeatText("x",3)
String



💡 Kural:
Yeni bir core modül yazdığınızda, mutlaka aynı isimle bir Binding oluşturun.
Her binding dosyasında:
	•	Register<Module>Binding()
	•	BindingRegistry kaydı
	•	Her fonksiyon için v8::Function::New() tanımı bulunmalı.