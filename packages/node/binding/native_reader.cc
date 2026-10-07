#include "native_reader.h"

#include "aviotrix/json.h"
#include "aviotrix/remux.h"

namespace aviotrix_node {

using aviotrix::ErrorCode;
using aviotrix::Status;

namespace {

// Validates the constructor argument before the bridge takes a reference to it.
Napi::Object requireHost(const Napi::CallbackInfo& info) {
  if (info.Length() < 1 || !info[0].IsObject())
    throw Napi::TypeError::New(info.Env(), "NativeReader(host) requires a host object");
  return info[0].As<Napi::Object>();
}

}  // namespace

Napi::Function NativeReader::Init(Napi::Env env) {
  return DefineClass(env, "NativeReader",
                     {InstanceMethod("open", &NativeReader::Open), InstanceMethod("remux", &NativeReader::Remux),
                      InstanceMethod("cancel", &NativeReader::Cancel), InstanceMethod("close", &NativeReader::Close)});
}

NativeReader::NativeReader(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<NativeReader>(info),
      bridge_(std::make_unique<MainThreadBridge>(info.Env(), requireHost(info), "aviotrix:reader")),
      source_(*bridge_) {
  worker_ = std::thread([this] { workerLoop(); });
}

NativeReader::~NativeReader() {
  shutdownWorker();
}

void NativeReader::workerLoop() {
  while (true) {
    std::function<void()> job;
    {
      std::unique_lock<std::mutex> lock(queueMutex_);
      queueCv_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
      if (stopping_ && queue_.empty()) return;
      job = std::move(queue_.front());
      queue_.pop_front();
    }
    job();
  }
}

void NativeReader::shutdownWorker() {
  if (shutDown_) return;
  shutDown_ = true;
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    stopping_ = true;
  }
  queueCv_.notify_all();
  if (worker_.joinable()) worker_.join();
  bridge_->release();
}

Napi::Value NativeReader::runOnWorker(Napi::Env env, std::function<std::pair<Status, std::string>()> work,
                                      std::function<void()> afterSettle) {
  auto deferred = std::make_shared<Napi::Promise::Deferred>(Napi::Promise::Deferred::New(env));
  if (closing_ || shutDown_) {
    Napi::Error err = Napi::Error::New(env, "reader is closed");
    err.Set("code", "NOT_OPEN");
    deferred->Reject(err.Value());
    return deferred->Promise();
  }
  Ref();                     // keep this wrapper alive while the worker uses it
  bridge_->keepAlive(true);  // keep the event loop alive while the worker runs
  auto settle = [this, afterSettle = std::move(afterSettle)]() {
    bridge_->keepAlive(false);
    if (afterSettle) afterSettle();
    Unref();
  };
  {
    std::lock_guard<std::mutex> lock(queueMutex_);
    queue_.emplace_back([this, deferred, work = std::move(work), settle = std::move(settle)]() mutable {
      auto [status, json] = work();
      bridge_->complete(Completion{deferred, std::move(status), std::move(json), std::move(settle)});
    });
  }
  queueCv_.notify_one();
  return deferred->Promise();
}

Napi::Value NativeReader::Open(const Napi::CallbackInfo& info) {
  return runOnWorker(info.Env(), [this]() -> std::pair<Status, std::string> {
    aviotrix::OpenOptions opts;
    opts.log = [this](aviotrix::LogLevel level, std::string_view text) {
      bridge_->log(aviotrix::logLevelName(level), std::string(text));
    };
    Status st = reader_.open(source_, std::move(opts));
    if (!st.ok()) return {st, ""};
    return {st, aviotrix::toJson(reader_.metadata())};
  });
}

Napi::Value NativeReader::Remux(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsObject())
    throw Napi::TypeError::New(env, "remux(options) requires an options object");
  Napi::Object o = info[0].As<Napi::Object>();

  aviotrix::RemuxOptions opt;
  opt.format = o.Get("format").ToString().Utf8Value();
  opt.failOnIncompatible = o.Get("failOnIncompatible").ToBoolean().Value();
  opt.fragmented = o.Get("fragmented").ToBoolean().Value();
  const bool sinkSeekable = o.Get("sinkSeekable").ToBoolean().Value();
  Napi::Value interval = o.Get("progressIntervalPackets");
  if (interval.IsNumber() && interval.As<Napi::Number>().Int32Value() > 0)
    opt.progressIntervalPackets = interval.As<Napi::Number>().Int32Value();
  Napi::Value streams = o.Get("streams");
  if (streams.IsArray()) {
    Napi::Array arr = streams.As<Napi::Array>();
    std::vector<int> idx;
    for (uint32_t i = 0; i < arr.Length(); i++) idx.push_back(arr.Get(i).ToNumber().Int32Value());
    opt.streams = std::move(idx);
  }
  opt.cancel = &cancel_;
  opt.onProgress = [this](const aviotrix::RemuxProgress& p) {
    bridge_->progress(p.bytesRead, p.bytesWritten, p.timestamp);
  };

  return runOnWorker(env, [this, opt = std::move(opt), sinkSeekable]() mutable -> std::pair<Status, std::string> {
    JsIoSink sink(*bridge_, sinkSeekable);
    aviotrix::RemuxResult result;
    Status st = reader_.remux(sink, opt, result);
    cancel_.store(false);
    if (!st.ok()) return {st, ""};
    return {st, aviotrix::toJson(result)};
  });
}

Napi::Value NativeReader::Cancel(const Napi::CallbackInfo& info) {
  cancel_.store(true);
  return info.Env().Undefined();
}

Napi::Value NativeReader::Close(const Napi::CallbackInfo& info) {
  Napi::Value promise = runOnWorker(
      info.Env(), [this]() -> std::pair<Status, std::string> { return {reader_.close(), ""}; },
      [this] { shutdownWorker(); });
  closing_ = true;
  return promise;
}

}  // namespace aviotrix_node
