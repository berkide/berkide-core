// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "CommandsBinding.h"
#include "BindingRegistry.h"
#include "EditorContext.h"
#include "V8Engine.h"
#include "Logger.h"
#include <v8.h>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// Register commands API on editor.commands with __nativeExec, register, exec, list methods
// editor.commands uzerine __nativeExec, register, exec, list metodlariyla komut API'sini kaydet
void RegisterCommandsBinding(v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& /*ctx*/) {
    auto v8ctx = isolate->GetCurrentContext();
    v8::Local<v8::Object> commands = v8::Object::New(isolate);

    // Native exec callback: routes JS command calls to the C++ CommandRouter
    // Native exec callback: JS komut cagrilarini C++ CommandRouter'a yonlendirir
    auto nativeExecFn = v8::Function::New(
                            v8ctx,
                            [](const v8::FunctionCallbackInfo<v8::Value> &info)
                            {
                                if (info.Length() < 1)
                                    return;
                                auto isolate = info.GetIsolate();
                                v8::HandleScope handle_scope(isolate);

                                v8::String::Utf8Value name(isolate, info[0]);
                                std::string cmdName = *name ? *name : "";

                                std::string argsStr = "{}";
                                if (info.Length() > 1 && info[1]->IsString())
                                {
                                    v8::String::Utf8Value argsUtf8(isolate, info[1]);
                                    argsStr = *argsUtf8 ? *argsUtf8 : "{}";
                                }

                                auto *self = static_cast<V8Engine *>(isolate->GetData(0));
                                std::string result = self->commandRouter().execFromJS(cmdName, argsStr);

                                info.GetReturnValue().Set(
                                    v8::String::NewFromUtf8(isolate, result.c_str()).ToLocalChecked());
                            })
                            .ToLocalChecked();

    commands->Set(v8ctx,
                  v8::String::NewFromUtf8Literal(isolate, "__nativeExec"),
                  nativeExecFn)
        .Check();

    commands->Set(v8ctx,
                  v8::String::NewFromUtf8Literal(isolate, "register"),
                  v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value> &info) {}).ToLocalChecked())
        .Check();

    commands->Set(v8ctx,
                  v8::String::NewFromUtf8Literal(isolate, "exec"),
                  v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value> &info) {}).ToLocalChecked())
        .Check();

    // commands.list() — return all registered command names as JS array
    // commands.list() — tum kayitli komut adlarini JS dizisi olarak dondur
    commands->Set(v8ctx,
                  v8::String::NewFromUtf8Literal(isolate, "list"),
                  v8::Function::New(v8ctx, [](const v8::FunctionCallbackInfo<v8::Value> &args) {
                      auto* isolate = args.GetIsolate();
                      auto ctx = isolate->GetCurrentContext();
                      auto* self = static_cast<V8Engine*>(isolate->GetData(0));
                      json all = self->listCommands();
                      v8::Local<v8::Array> arr = v8::Array::New(isolate);
                      uint32_t idx = 0;
                      for (auto& name : all["commands"]) {
                          arr->Set(ctx, idx++,
                              v8::String::NewFromUtf8(isolate, name.get<std::string>().c_str()).ToLocalChecked()).Check();
                      }
                      for (auto& name : all["queries"]) {
                          arr->Set(ctx, idx++,
                              v8::String::NewFromUtf8(isolate, name.get<std::string>().c_str()).ToLocalChecked()).Check();
                      }
                      args.GetReturnValue().Set(arr);
                  }).ToLocalChecked())
        .Check();

    editorObj->Set(v8ctx,
                v8::String::NewFromUtf8Literal(isolate, "commands"),
                commands)
        .Check();
}

// Auto-register "commands" binding at static init time
// "commands" binding'ini statik baslangicta otomatik kaydet
static bool registered_commands = []{
    BindingRegistry::instance().registerBinding("commands",
        [](v8::Isolate* isolate, v8::Local<v8::Object> editorObj, EditorContext& ctx){
            RegisterCommandsBinding(isolate, editorObj, ctx);
        });
    return true;
}();
