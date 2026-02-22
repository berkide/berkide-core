// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "WorkerManager.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include "v8.h"
#include "libplatform/libplatform.h"

// Constructor
// Kurucu
WorkerManager::WorkerManager() {}

// Destructor: terminate all workers on shutdown
// Yikici: kapatmada tum calisanlari sonlandir
WorkerManager::~WorkerManager() {
    terminateAll();
}

// Read a script file to a string
// Betik dosyasini dizeye oku
static std::string readScriptFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Create a new worker that runs a script file
// Bir betik dosyasi calistiran yeni calisan olustur
int WorkerManager::createWorker(const std::string& scriptPath) {
    std::string source = readScriptFile(scriptPath);
    if (source.empty()) {
        LOG_ERROR("[Worker] Cannot read script: ", scriptPath);
        return -1;
    }

    int id = nextId_.fetch_add(1);
    auto w = std::make_unique<Worker>();
    w->id = id;
    w->scriptPath = scriptPath;
    w->scriptSource = std::move(source);

    Worker* rawPtr = w.get();
    w->thread = std::thread(&WorkerManager::workerThreadFunc, this, rawPtr);

    std::lock_guard<std::mutex> lock(mutex_);
    workers_[id] = std::move(w);

    LOG_INFO("[Worker] Created worker #", id, " from file: ", scriptPath);
    return id;
}

// Create a worker from inline script source
// Satir ici betik kaynagindan calisan olustur
int WorkerManager::createWorkerFromSource(const std::string& source) {
    if (source.empty()) return -1;

    int id = nextId_.fetch_add(1);
    auto w = std::make_unique<Worker>();
    w->id = id;
    w->scriptSource = source;

    Worker* rawPtr = w.get();
    w->thread = std::thread(&WorkerManager::workerThreadFunc, this, rawPtr);

    std::lock_guard<std::mutex> lock(mutex_);
    workers_[id] = std::move(w);

    LOG_INFO("[Worker] Created worker #", id, " from source");
    return id;
}

// Post a message to a worker's inbound queue
// Bir calisanin gelen kuyruguna mesaj gonder
bool WorkerManager::postMessage(int workerId, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = workers_.find(workerId);
    if (it == workers_.end()) return false;

    Worker* w = it->second.get();
    if (w->state.load() != WorkerState::Running) return false;

    {
        std::lock_guard<std::mutex> inLock(w->inMutex);
        w->inQueue.push(message);
    }
    w->inCv.notify_one();
    return true;
}

// Terminate a specific worker
// Belirli bir calisani sonlandir
bool WorkerManager::terminate(int workerId) {
    std::unique_ptr<Worker> w;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = workers_.find(workerId);
        if (it == workers_.end()) return false;
        w = std::move(it->second);
        workers_.erase(it);
    }

    // Signal the worker to stop and wake it up
    // Calisana durmasini isaretle ve uyandır
    w->shouldStop.store(true);
    w->inCv.notify_all();

    if (w->thread.joinable()) {
        w->thread.join();
    }

    LOG_INFO("[Worker] Terminated worker #", workerId);
    return true;
}

// Terminate all workers
// Tum calisanlari sonlandir
void WorkerManager::terminateAll() {
    std::unordered_map<int, std::unique_ptr<Worker>> copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        copy = std::move(workers_);
        workers_.clear();
    }

    for (auto& [id, w] : copy) {
        w->shouldStop.store(true);
        w->inCv.notify_all();
    }
    for (auto& [id, w] : copy) {
        if (w->thread.joinable()) {
            w->thread.join();
        }
    }

    if (!copy.empty()) {
        LOG_INFO("[Worker] Terminated all workers (", copy.size(), ")");
    }
}

// Get state of a worker
// Bir calisanin durumunu al
WorkerState WorkerManager::getState(int workerId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = workers_.find(workerId);
    if (it == workers_.end()) return WorkerState::Stopped;
    return it->second->state.load();
}

// Get number of active (Running) workers
// Aktif (Calisan) calisan sayisini al
int WorkerManager::activeCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int count = 0;
    for (auto& [id, w] : workers_) {
        if (w->state.load() == WorkerState::Running) ++count;
    }
    return count;
}

// Set callback for messages from workers to main thread
// Calisanlardan ana thread'e mesajlar icin geri cagirim ayarla
void WorkerManager::setMessageCallback(WorkerMessageCallback cb) {
    std::lock_guard<std::mutex> lock(outMutex_);
    messageCallback_ = std::move(cb);
}

// Process pending messages from workers (call from main thread event loop)
// Calisanlardan bekleyen mesajlari isle (ana thread olay dongusunden cagir)
void WorkerManager::processPendingMessages() {
    std::queue<WorkerMessage> pending;
    {
        std::lock_guard<std::mutex> lock(outMutex_);
        std::swap(pending, outQueue_);
    }

    WorkerMessageCallback cb;
    {
        std::lock_guard<std::mutex> lock(outMutex_);
        cb = messageCallback_;
    }

    while (!pending.empty()) {
        WorkerMessage msg = std::move(pending.front());
        pending.pop();
        if (cb) {
            cb(msg.workerId, msg.data);
        }
    }
}

// Post message from worker thread to main thread outbound queue
// Calisan thread'inden ana thread giden kuyruguna mesaj gonder
void WorkerManager::postToMain(int workerId, const std::string& message) {
    std::lock_guard<std::mutex> lock(outMutex_);
    outQueue_.push({message, workerId});
}

// Worker thread entry point: creates its own V8 isolate and runs the script
// Calisan thread giris noktasi: kendi V8 izolasyonunu olusturur ve betigi calistirir
void WorkerManager::workerThreadFunc(Worker* worker) {
    worker->state.store(WorkerState::Running);

    // Create a new V8 isolate for this worker (true parallelism)
    // Bu calisan icin yeni V8 izolasyonu olustur (gercek paralellik)
    v8::Isolate::CreateParams createParams;
    createParams.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    v8::Isolate* isolate = v8::Isolate::New(createParams);

    {
        v8::Isolate::Scope isolateScope(isolate);
        v8::HandleScope handleScope(isolate);
        v8::Local<v8::Context> context = v8::Context::New(isolate);
        v8::Context::Scope contextScope(context);

        // Inject postMessage(data) global function for worker -> main communication
        // Calisan -> ana iletisimi icin global postMessage(data) fonksiyonunu enjekte et
        int workerId = worker->id;
        WorkerManager* mgr = this;

        auto postMsgTmpl = v8::FunctionTemplate::New(isolate,
            [](const v8::FunctionCallbackInfo<v8::Value>& args) {
                auto iso = args.GetIsolate();
                if (args.Length() < 1) return;
                v8::String::Utf8Value msg(iso, args[0]);
                if (!*msg) return;

                // Retrieve workerId and manager from external data
                // Dis veriden calisan ID'si ve yoneticiyi al
                auto ext = args.Data().As<v8::External>();
                auto* pair = static_cast<std::pair<WorkerManager*, int>*>(ext->Value());
                pair->first->postToMain(pair->second, *msg);
            },
            v8::External::New(isolate, new std::pair<WorkerManager*, int>(mgr, workerId))
        );
        context->Global()->Set(context,
            v8::String::NewFromUtf8Literal(isolate, "postMessage"),
            postMsgTmpl->GetFunction(context).ToLocalChecked()).Check();

        // Inject console.log for debugging
        // Hata ayiklama icin console.log enjekte et
        auto consoleTmpl = v8::ObjectTemplate::New(isolate);
        consoleTmpl->Set(isolate, "log", v8::FunctionTemplate::New(isolate,
            [](const v8::FunctionCallbackInfo<v8::Value>& args) {
                auto iso = args.GetIsolate();
                std::string out;
                for (int i = 0; i < args.Length(); ++i) {
                    if (i > 0) out += " ";
                    v8::String::Utf8Value s(iso, args[i]);
                    out += *s ? *s : "(null)";
                }
                LOG_INFO("[Worker] ", out);
            }));
        context->Global()->Set(context,
            v8::String::NewFromUtf8Literal(isolate, "console"),
            consoleTmpl->NewInstance(context).ToLocalChecked()).Check();

        // Inject onmessage handler registration (worker sets self.onmessage = fn)
        // onmessage isleyici kaydini enjekte et (calisan self.onmessage = fn ayarlar)
        context->Global()->Set(context,
            v8::String::NewFromUtf8Literal(isolate, "self"),
            context->Global()).Check();

        // Run the worker script
        // Calisan betigini calistir
        v8::Local<v8::String> sourceStr = v8::String::NewFromUtf8(
            isolate, worker->scriptSource.c_str()).ToLocalChecked();

        v8::TryCatch tryCatch(isolate);
        v8::Local<v8::Script> script;
        if (!v8::Script::Compile(context, sourceStr).ToLocal(&script)) {
            v8::String::Utf8Value err(isolate, tryCatch.Exception());
            LOG_ERROR("[Worker #", workerId, "] Compile error: ", *err ? *err : "unknown");
            worker->state.store(WorkerState::Error);
            isolate->Dispose();
            delete createParams.array_buffer_allocator;
            return;
        }

        v8::Local<v8::Value> result;
        if (!script->Run(context).ToLocal(&result)) {
            v8::String::Utf8Value err(isolate, tryCatch.Exception());
            LOG_ERROR("[Worker #", workerId, "] Runtime error: ", *err ? *err : "unknown");
            worker->state.store(WorkerState::Error);
            isolate->Dispose();
            delete createParams.array_buffer_allocator;
            return;
        }

        // Message loop: wait for inbound messages and call self.onmessage(data)
        // Mesaj dongusu: gelen mesajlari bekle ve self.onmessage(data) cagir
        while (!worker->shouldStop.load()) {
            std::string msg;
            {
                std::unique_lock<std::mutex> lock(worker->inMutex);
                worker->inCv.wait_for(lock, std::chrono::milliseconds(100),
                    [&] { return !worker->inQueue.empty() || worker->shouldStop.load(); });

                if (worker->shouldStop.load()) break;
                if (worker->inQueue.empty()) continue;

                msg = std::move(worker->inQueue.front());
                worker->inQueue.pop();
            }

            // Call self.onmessage if defined
            // Tanimlanmissa self.onmessage'i cagir
            v8::HandleScope loopScope(isolate);
            v8::Local<v8::Value> onmsgVal;
            if (context->Global()->Get(context,
                    v8::String::NewFromUtf8Literal(isolate, "onmessage")).ToLocal(&onmsgVal)
                && onmsgVal->IsFunction()) {

                v8::Local<v8::Function> onmsgFn = onmsgVal.As<v8::Function>();
                v8::Local<v8::String> dataStr = v8::String::NewFromUtf8(
                    isolate, msg.c_str()).ToLocalChecked();

                // Create {data: "..."} event object
                // {data: "..."} olay nesnesi olustur
                v8::Local<v8::Object> eventObj = v8::Object::New(isolate);
                eventObj->Set(context,
                    v8::String::NewFromUtf8Literal(isolate, "data"),
                    dataStr).Check();

                v8::Local<v8::Value> argv[] = { eventObj };
                v8::TryCatch loopTry(isolate);
                onmsgFn->Call(context, context->Global(), 1, argv);
                if (loopTry.HasCaught()) {
                    v8::String::Utf8Value err(isolate, loopTry.Exception());
                    LOG_ERROR("[Worker #", workerId, "] onmessage error: ", *err ? *err : "unknown");
                }
            }
        }
    }

    // Cleanup V8 isolate
    // V8 izolasyonunu temizle
    isolate->Dispose();
    delete createParams.array_buffer_allocator;
    worker->state.store(WorkerState::Stopped);
    LOG_INFO("[Worker #", worker->id, "] Thread exited");
}
