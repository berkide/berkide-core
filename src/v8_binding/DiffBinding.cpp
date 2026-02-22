// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "DiffBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8ResponseBuilder.h"
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

// Context for diff binding
// Diff binding baglami
struct DiffCtx {
    DiffEngine* engine;
    I18n* i18n;
};

// Helper: convert DiffHunk to nlohmann::json
// Yardimci: DiffHunk'i nlohmann::json'a cevir
static json hunkToJson(const DiffHunk& h) {
    const char* typeStr = "equal";
    switch (h.type) {
        case DiffType::Insert:  typeStr = "insert"; break;
        case DiffType::Delete:  typeStr = "delete"; break;
        case DiffType::Replace: typeStr = "replace"; break;
        default: break;
    }

    json oldLines = json::array();
    for (const auto& l : h.oldLines) oldLines.push_back(l);

    json newLines = json::array();
    for (const auto& l : h.newLines) newLines.push_back(l);

    return json({
        {"type", typeStr},
        {"oldStart", h.oldStart},
        {"oldCount", h.oldCount},
        {"newStart", h.newStart},
        {"newCount", h.newCount},
        {"oldLines", oldLines},
        {"newLines", newLines}
    });
}

// Helper: reconstruct DiffHunk vector from JS array
// Yardimci: JS dizisinden DiffHunk vektoru olustur
static std::vector<DiffHunk> parseHunksFromJS(v8::Isolate* iso, v8::Local<v8::Context> ctx,
                                                v8::Local<v8::Array> jsArr) {
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

        // Parse type string if present
        // Varsa tur dizesini ayristir
        auto typeVal = jsH->Get(ctx, v8::String::NewFromUtf8Literal(iso, "type"));
        if (!typeVal.IsEmpty()) {
            v8::String::Utf8Value typeUtf(iso, typeVal.ToLocalChecked());
            std::string typeStr = *typeUtf ? *typeUtf : "equal";
            if (typeStr == "insert")       h.type = DiffType::Insert;
            else if (typeStr == "delete")  h.type = DiffType::Delete;
            else if (typeStr == "replace") h.type = DiffType::Replace;
            else                           h.type = DiffType::Equal;
        }

        hunks.push_back(std::move(h));
    }
    return hunks;
}

// Register editor.diff JS object with standard response format
// Standart yanit formatiyla editor.diff JS nesnesini kaydet
void RegisterDiffBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& edCtx) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> jsDiff = v8::Object::New(isolate);

    auto* dctx = new DiffCtx{edCtx.diffEngine, edCtx.i18n};

    // diff.compute(oldLines, newLines) -> {ok, data: [hunk, ...], meta: {total: N}}
    // Iki satir dizisi arasinda diff hesapla
    jsDiff->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "compute"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* dc = static_cast<DiffCtx*>(args.Data().As<v8::External>()->Value());
            if (!dc || !dc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "diffEngine"}}, dc ? dc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "oldLines, newLines"}}, dc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            auto oldLines = v8ArrayToStrVec(iso, ctx, args[0]);
            auto newLines = v8ArrayToStrVec(iso, ctx, args[1]);

            auto hunks = dc->engine->diff(oldLines, newLines);
            json arr = json::array();
            for (const auto& h : hunks) {
                arr.push_back(hunkToJson(h));
            }
            json meta = {{"total", hunks.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, dctx)).ToLocalChecked()
    ).Check();

    // diff.computeText(oldText, newText) -> {ok, data: [hunk, ...], meta: {total: N}}
    // Iki metin dizesi arasinda diff hesapla
    jsDiff->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "computeText"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* dc = static_cast<DiffCtx*>(args.Data().As<v8::External>()->Value());
            if (!dc || !dc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "diffEngine"}}, dc ? dc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "oldText, newText"}}, dc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();

            std::string oldText = v8Str(iso, args[0]);
            std::string newText = v8Str(iso, args[1]);

            auto hunks = dc->engine->diffText(oldText, newText);
            json arr = json::array();
            for (const auto& h : hunks) {
                arr.push_back(hunkToJson(h));
            }
            json meta = {{"total", hunks.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, dctx)).ToLocalChecked()
    ).Check();

    // diff.unified(hunks, oldName?, newName?) -> {ok, data: string}
    // Birlesik diff formati olustur
    jsDiff->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "unified"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* dc = static_cast<DiffCtx*>(args.Data().As<v8::External>()->Value());
            if (!dc || !dc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "diffEngine"}}, dc ? dc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1 || !args[0]->IsArray()) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "hunks"}}, dc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            auto hunks = parseHunksFromJS(iso, ctx, args[0].As<v8::Array>());

            std::string oldName = (args.Length() > 1) ? v8Str(iso, args[1]) : "a";
            std::string newName = (args.Length() > 2) ? v8Str(iso, args[2]) : "b";

            std::string result = dc->engine->unifiedDiff(hunks, oldName, newName);
            V8Response::ok(args, result);
        }, v8::External::New(isolate, dctx)).ToLocalChecked()
    ).Check();

    // diff.merge3(base, ours, theirs) -> {ok, data: {lines, hasConflicts, conflictCount}}
    // Uc yonlu birlestirme
    jsDiff->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "merge3"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* dc = static_cast<DiffCtx*>(args.Data().As<v8::External>()->Value());
            if (!dc || !dc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "diffEngine"}}, dc ? dc->i18n : nullptr);
                return;
            }
            if (args.Length() < 3) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "base, ours, theirs"}}, dc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            auto base   = v8ArrayToStrVec(iso, ctx, args[0]);
            auto ours   = v8ArrayToStrVec(iso, ctx, args[1]);
            auto theirs = v8ArrayToStrVec(iso, ctx, args[2]);

            auto result = dc->engine->merge3(base, ours, theirs);

            json linesArr = json::array();
            for (const auto& l : result.lines) {
                linesArr.push_back(l);
            }

            json data = {
                {"lines", linesArr},
                {"hasConflicts", result.hasConflicts},
                {"conflictCount", result.conflictCount}
            };
            V8Response::ok(args, data);
        }, v8::External::New(isolate, dctx)).ToLocalChecked()
    ).Check();

    // diff.applyPatch(originalLines, hunks) -> {ok, data: [string, ...], meta: {total: N}}
    // Orijinal satirlara yamayi uygula
    jsDiff->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "applyPatch"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* dc = static_cast<DiffCtx*>(args.Data().As<v8::External>()->Value());
            if (!dc || !dc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "diffEngine"}}, dc ? dc->i18n : nullptr);
                return;
            }
            if (args.Length() < 2 || !args[0]->IsArray() || !args[1]->IsArray()) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "originalLines, hunks"}}, dc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            // Parse original lines
            // Orijinal satirlari ayristir
            auto original = v8ArrayToStrVec(iso, ctx, args[0]);

            // Parse hunks from JS array
            // JS dizisinden parcalari ayristir
            auto hunks = parseHunksFromJS(iso, ctx, args[1].As<v8::Array>());

            auto result = dc->engine->applyPatch(original, hunks);
            json arr = json::array();
            for (const auto& l : result) {
                arr.push_back(l);
            }
            json meta = {{"total", result.size()}};
            V8Response::ok(args, arr, meta);
        }, v8::External::New(isolate, dctx)).ToLocalChecked()
    ).Check();

    // diff.countInsertions(hunks) -> {ok, data: int}
    // Diff parcalarindaki eklemeleri say
    jsDiff->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "countInsertions"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* dc = static_cast<DiffCtx*>(args.Data().As<v8::External>()->Value());
            if (!dc || !dc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "diffEngine"}}, dc ? dc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1 || !args[0]->IsArray()) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "hunks"}}, dc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            auto hunks = parseHunksFromJS(iso, ctx, args[0].As<v8::Array>());
            int count = dc->engine->countInsertions(hunks);
            V8Response::ok(args, count);
        }, v8::External::New(isolate, dctx)).ToLocalChecked()
    ).Check();

    // diff.countDeletions(hunks) -> {ok, data: int}
    // Diff parcalarindaki silmeleri say
    jsDiff->Set(v8ctx,
        v8::String::NewFromUtf8Literal(isolate, "countDeletions"),
        v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
            auto* dc = static_cast<DiffCtx*>(args.Data().As<v8::External>()->Value());
            if (!dc || !dc->engine) {
                V8Response::error(args, "NULL_CONTEXT", "internal.null_manager",
                    {{"name", "diffEngine"}}, dc ? dc->i18n : nullptr);
                return;
            }
            if (args.Length() < 1 || !args[0]->IsArray()) {
                V8Response::error(args, "MISSING_ARG", "args.missing",
                    {{"name", "hunks"}}, dc->i18n);
                return;
            }
            auto* iso = args.GetIsolate();
            auto ctx = iso->GetCurrentContext();

            auto hunks = parseHunksFromJS(iso, ctx, args[0].As<v8::Array>());
            int count = dc->engine->countDeletions(hunks);
            V8Response::ok(args, count);
        }, v8::External::New(isolate, dctx)).ToLocalChecked()
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
