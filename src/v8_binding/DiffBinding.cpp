#include "DiffBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "DiffEngine.h"
#include <v8.h>

// Helper: extract string from V8 value
// Yardimci: V8 degerinden string cikar
static std::string v8Str(v8::Isolate* iso, v8::Local<v8::Value> val) {
    v8::String::Utf8Value s(iso, val);
    return *s ? *s : "";
}

// Helper: V8 string array to vector<string>
// Yardimci: V8 dize dizisini vector<string>'e cevir
static std::vector<std::string> v8ArrayToStrVec(v8::Isolate* iso, v8::Local<v8::Context> ctx,
                                                  v8::Local<v8::Value> val) {
    std::vector<std::string> result;
    if (!val->IsArray()) return result;
    auto arr = val.As<v8::Array>();
    result.reserve(arr->Length());
    for (uint32_t i = 0; i < arr->Length(); ++i) {
        auto elem = arr->Get(ctx, i).ToLocalChecked();
        result.push_back(v8Str(iso, elem));
    }
    return result;
}

// Helper: convert DiffHunk to V8 object
// Yardimci: DiffHunk'i V8 nesnesine cevir
static v8::Local<v8::Object> hunkToV8(v8::Isolate* iso, v8::Local<v8::Context> ctx,
                                        const DiffHunk& h) {
    v8::Local<v8::Object> obj = v8::Object::New(iso);

    const char* typeStr = "equal";
    switch (h.type) {
        case DiffType::Insert:  typeStr = "insert"; break;
        case DiffType::Delete:  typeStr = "delete"; break;
        case DiffType::Replace: typeStr = "replace"; break;
        default: break;
    }

    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "type"),
        v8::String::NewFromUtf8(iso, typeStr).ToLocalChecked()).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "oldStart"),
        v8::Integer::New(iso, h.oldStart)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "oldCount"),
        v8::Integer::New(iso, h.oldCount)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "newStart"),
        v8::Integer::New(iso, h.newStart)).Check();
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "newCount"),
        v8::Integer::New(iso, h.newCount)).Check();

    // Old lines array
    // Eski satirlar dizisi
    v8::Local<v8::Array> oldArr = v8::Array::New(iso, static_cast<int>(h.oldLines.size()));
    for (size_t i = 0; i < h.oldLines.size(); ++i) {
        oldArr->Set(ctx, static_cast<uint32_t>(i),
            v8::String::NewFromUtf8(iso, h.oldLines[i].c_str()).ToLocalChecked()).Check();
    }
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "oldLines"), oldArr).Check();

    // New lines array
    // Yeni satirlar dizisi
    v8::Local<v8::Array> newArr = v8::Array::New(iso, static_cast<int>(h.newLines.size()));
    for (size_t i = 0; i < h.newLines.size(); ++i) {
        newArr->Set(ctx, static_cast<uint32_t>(i),
            v8::String::NewFromUtf8(iso, h.newLines[i].c_str()).ToLocalChecked()).Check();
    }
    obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "newLines"), newArr).Check();

    return obj;
}

// Register editor.diff JS object
// editor.diff JS nesnesini kaydet
void RegisterDiffBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsDiff = v8::Object::New(isolate);

    auto* eng = edCtx.diffEngine;

    // diff.compute(oldLines, newLines) -> [hunk, ...]
    // Iki satir dizisi arasinda diff hesapla
    jsDiff->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "compute"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* d = static_cast<DiffEngine*>(args.Data().As<v8::External>()->Value());
            if (!d || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            auto oldLines = v8ArrayToStrVec(iso, ctx, args[0]);
            auto newLines = v8ArrayToStrVec(iso, ctx, args[1]);

            auto hunks = d->diff(oldLines, newLines);
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(hunks.size()));
            for (size_t i = 0; i < hunks.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i), hunkToV8(iso, ctx, hunks[i])).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, eng)).ToLocalChecked()
    ).Check();

    // diff.computeText(oldText, newText) -> [hunk, ...]
    // Iki metin dizesi arasinda diff hesapla
    jsDiff->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "computeText"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* d = static_cast<DiffEngine*>(args.Data().As<v8::External>()->Value());
            if (!d || args.Length() < 2) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            std::string oldText = v8Str(iso, args[0]);
            std::string newText = v8Str(iso, args[1]);

            auto hunks = d->diffText(oldText, newText);
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(hunks.size()));
            for (size_t i = 0; i < hunks.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i), hunkToV8(iso, ctx, hunks[i])).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, eng)).ToLocalChecked()
    ).Check();

    // diff.unified(hunks, oldName?, newName?) -> string
    // Birlesik diff formati olustur
    jsDiff->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "unified"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* d = static_cast<DiffEngine*>(args.Data().As<v8::External>()->Value());
            if (!d || args.Length() < 1 || !args[0]->IsArray()) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            // Reconstruct hunks from JS array
            // JS dizisinden parclari yeniden olustur
            auto jsArr = args[0].As<v8::Array>();
            std::vector<DiffHunk> hunks;
            for (uint32_t i = 0; i < jsArr->Length(); ++i) {
                auto jsH = jsArr->Get(ctx, i).ToLocalChecked().As<v8::Object>();
                DiffHunk h;
                h.oldStart = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "oldStart"))
                    .ToLocalChecked()->Int32Value(ctx).FromJust();
                h.oldCount = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "oldCount"))
                    .ToLocalChecked()->Int32Value(ctx).FromJust();
                h.newStart = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "newStart"))
                    .ToLocalChecked()->Int32Value(ctx).FromJust();
                h.newCount = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "newCount"))
                    .ToLocalChecked()->Int32Value(ctx).FromJust();
                h.oldLines = v8ArrayToStrVec(iso, ctx,
                    jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "oldLines")).ToLocalChecked());
                h.newLines = v8ArrayToStrVec(iso, ctx,
                    jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "newLines")).ToLocalChecked());
                hunks.push_back(std::move(h));
            }

            std::string oldName = (args.Length() > 1) ? v8Str(iso, args[1]) : "a";
            std::string newName = (args.Length() > 2) ? v8Str(iso, args[2]) : "b";

            std::string result = d->unifiedDiff(hunks, oldName, newName);
            args.GetReturnValue().Set(
                v8::String::NewFromUtf8(iso, result.c_str()).ToLocalChecked());
        }, v8::External::New(isolate, eng)).ToLocalChecked()
    ).Check();

    // diff.merge3(base, ours, theirs) -> {lines, hasConflicts, conflictCount}
    // Uc yonlu birlestirme
    jsDiff->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "merge3"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* d = static_cast<DiffEngine*>(args.Data().As<v8::External>()->Value());
            if (!d || args.Length() < 3) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            auto base   = v8ArrayToStrVec(iso, ctx, args[0]);
            auto ours   = v8ArrayToStrVec(iso, ctx, args[1]);
            auto theirs = v8ArrayToStrVec(iso, ctx, args[2]);

            auto result = d->merge3(base, ours, theirs);

            v8::Local<v8::Object> obj = v8::Object::New(iso);
            v8::Local<v8::Array> linesArr = v8::Array::New(iso, static_cast<int>(result.lines.size()));
            for (size_t i = 0; i < result.lines.size(); ++i) {
                linesArr->Set(ctx, static_cast<uint32_t>(i),
                    v8::String::NewFromUtf8(iso, result.lines[i].c_str()).ToLocalChecked()).Check();
            }
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "lines"), linesArr).Check();
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "hasConflicts"),
                v8::Boolean::New(iso, result.hasConflicts)).Check();
            obj->Set(ctx, v8::String::NewFromUtf8Literal(iso, "conflictCount"),
                v8::Integer::New(iso, result.conflictCount)).Check();
            args.GetReturnValue().Set(obj);
        }, v8::External::New(isolate, eng)).ToLocalChecked()
    ).Check();

    // diff.applyPatch(originalLines, hunks) -> [string, ...] (patched lines)
    // Orijinal satirlara yamayi uygula
    jsDiff->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "applyPatch"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* d = static_cast<DiffEngine*>(args.Data().As<v8::External>()->Value());
            if (!d || args.Length() < 2 || !args[0]->IsArray() || !args[1]->IsArray()) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            // Parse original lines
            // Orijinal satirlari ayristir
            auto original = v8ArrayToStrVec(iso, ctx, args[0]);

            // Parse hunks from JS array
            // JS dizisinden parcalari ayristir
            auto jsArr = args[1].As<v8::Array>();
            std::vector<DiffHunk> hunks;
            for (uint32_t i = 0; i < jsArr->Length(); ++i) {
                auto jsH = jsArr->Get(ctx, i).ToLocalChecked().As<v8::Object>();
                DiffHunk h;
                h.oldStart = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "oldStart"))
                    .ToLocalChecked()->Int32Value(ctx).FromJust();
                h.oldCount = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "oldCount"))
                    .ToLocalChecked()->Int32Value(ctx).FromJust();
                h.newStart = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "newStart"))
                    .ToLocalChecked()->Int32Value(ctx).FromJust();
                h.newCount = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "newCount"))
                    .ToLocalChecked()->Int32Value(ctx).FromJust();
                h.oldLines = v8ArrayToStrVec(iso, ctx,
                    jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "oldLines")).ToLocalChecked());
                h.newLines = v8ArrayToStrVec(iso, ctx,
                    jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "newLines")).ToLocalChecked());

                // Parse type string
                // Tur dizesini ayristir
                v8::String::Utf8Value typeVal(iso,
                    jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "type")).ToLocalChecked());
                std::string typeStr = *typeVal ? *typeVal : "equal";
                if (typeStr == "insert")       h.type = DiffType::Insert;
                else if (typeStr == "delete")  h.type = DiffType::Delete;
                else if (typeStr == "replace") h.type = DiffType::Replace;
                else                           h.type = DiffType::Equal;

                hunks.push_back(std::move(h));
            }

            auto result = d->applyPatch(original, hunks);
            v8::Local<v8::Array> arr = v8::Array::New(iso, static_cast<int>(result.size()));
            for (size_t i = 0; i < result.size(); ++i) {
                arr->Set(ctx, static_cast<uint32_t>(i),
                    v8::String::NewFromUtf8(iso, result[i].c_str()).ToLocalChecked()).Check();
            }
            args.GetReturnValue().Set(arr);
        }, v8::External::New(isolate, eng)).ToLocalChecked()
    ).Check();

    // diff.countInsertions(hunks) -> int
    // Diff parcalarindaki eklemeleri say
    jsDiff->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "countInsertions"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* d = static_cast<DiffEngine*>(args.Data().As<v8::External>()->Value());
            if (!d || args.Length() < 1 || !args[0]->IsArray()) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            // Reconstruct hunks from JS array
            // JS dizisinden parcalari yeniden olustur
            auto jsArr = args[0].As<v8::Array>();
            std::vector<DiffHunk> hunks;
            for (uint32_t i = 0; i < jsArr->Length(); ++i) {
                auto jsH = jsArr->Get(ctx, i).ToLocalChecked().As<v8::Object>();
                DiffHunk h;
                h.oldStart = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "oldStart"))
                    .ToLocalChecked()->Int32Value(ctx).FromJust();
                h.oldCount = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "oldCount"))
                    .ToLocalChecked()->Int32Value(ctx).FromJust();
                h.newStart = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "newStart"))
                    .ToLocalChecked()->Int32Value(ctx).FromJust();
                h.newCount = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "newCount"))
                    .ToLocalChecked()->Int32Value(ctx).FromJust();
                h.oldLines = v8ArrayToStrVec(iso, ctx,
                    jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "oldLines")).ToLocalChecked());
                h.newLines = v8ArrayToStrVec(iso, ctx,
                    jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "newLines")).ToLocalChecked());
                hunks.push_back(std::move(h));
            }

            int count = d->countInsertions(hunks);
            args.GetReturnValue().Set(count);
        }, v8::External::New(isolate, eng)).ToLocalChecked()
    ).Check();

    // diff.countDeletions(hunks) -> int
    // Diff parcalarindaki silmeleri say
    jsDiff->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "countDeletions"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* d = static_cast<DiffEngine*>(args.Data().As<v8::External>()->Value());
            if (!d || args.Length() < 1 || !args[0]->IsArray()) return;
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            // Reconstruct hunks from JS array
            // JS dizisinden parcalari yeniden olustur
            auto jsArr = args[0].As<v8::Array>();
            std::vector<DiffHunk> hunks;
            for (uint32_t i = 0; i < jsArr->Length(); ++i) {
                auto jsH = jsArr->Get(ctx, i).ToLocalChecked().As<v8::Object>();
                DiffHunk h;
                h.oldStart = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "oldStart"))
                    .ToLocalChecked()->Int32Value(ctx).FromJust();
                h.oldCount = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "oldCount"))
                    .ToLocalChecked()->Int32Value(ctx).FromJust();
                h.newStart = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "newStart"))
                    .ToLocalChecked()->Int32Value(ctx).FromJust();
                h.newCount = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "newCount"))
                    .ToLocalChecked()->Int32Value(ctx).FromJust();
                h.oldLines = v8ArrayToStrVec(iso, ctx,
                    jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "oldLines")).ToLocalChecked());
                h.newLines = v8ArrayToStrVec(iso, ctx,
                    jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "newLines")).ToLocalChecked());
                hunks.push_back(std::move(h));
            }

            int count = d->countDeletions(hunks);
            args.GetReturnValue().Set(count);
        }, v8::External::New(isolate, eng)).ToLocalChecked()
    ).Check();

    editorObj->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "diff"),
        jsDiff).Check();
}

// Auto-register with BindingRegistry
// BindingRegistry'ye otomatik kaydet
static bool _diffReg = [] {
    BindingRegistry::instance().registerBinding("diff", RegisterDiffBinding);
    return true;
}();
