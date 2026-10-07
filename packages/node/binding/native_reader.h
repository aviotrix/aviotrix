#pragma once

#include <napi.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

#include "aviotrix/media_reader.h"
#include "js_io.h"
#include "main_thread_bridge.h"

namespace aviotrix_node {

class NativeReader : public Napi::ObjectWrap<NativeReader> {
 public:
  static Napi::Function Init(Napi::Env env);
  explicit NativeReader(const Napi::CallbackInfo& info);
  ~NativeReader() override;

 private:
  Napi::Value Open(const Napi::CallbackInfo& info);
  Napi::Value Remux(const Napi::CallbackInfo& info);
  Napi::Value Cancel(const Napi::CallbackInfo& info);
  Napi::Value Close(const Napi::CallbackInfo& info);

  // Runs `work` on the worker thread; `work` returns (status, json). Returns the JS Promise.
  Napi::Value runOnWorker(Napi::Env env, std::function<std::pair<aviotrix::Status, std::string>()> work,
                          std::function<void()> afterSettle = {});
  void workerLoop();
  void shutdownWorker();  // main thread; joins the thread and releases the bridge

  std::unique_ptr<MainThreadBridge> bridge_;
  JsIoSource source_;
  aviotrix::MediaReader reader_;
  std::atomic<bool> cancel_{false};

  std::thread worker_;
  std::mutex queueMutex_;
  std::condition_variable queueCv_;
  std::deque<std::function<void()>> queue_;
  bool stopping_ = false;
  // Main thread: close() has been queued. Later operations are rejected instead of queued: the close
  // completion joins the worker on the main thread, so a job queued behind it that called back into JS
  // would deadlock.
  bool closing_ = false;
  bool shutDown_ = false;
};

}  // namespace aviotrix_node
