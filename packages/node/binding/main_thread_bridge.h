#pragma once

#include <napi.h>

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <variant>

#include "aviotrix/status.h"

namespace aviotrix_node {

// One IO round trip from the worker thread to a NativeHost method on the JS main thread.
struct IoRequest {
  enum class Kind { SourceOpen, SourceRead, SourceClose, SinkOpen, SinkWrite, SinkClose };
  explicit IoRequest(Kind k) : kind(k) {}
  IoRequest(const IoRequest&) = delete;
  IoRequest& operator=(const IoRequest&) = delete;
  Kind kind;
  int64_t offset = 0;
  std::span<uint8_t> readInto;         // SourceRead: destination (its size() is the max length)
  std::span<const uint8_t> writeFrom;  // SinkWrite: payload
  // Results
  std::optional<int64_t> size;  // SourceOpen
  size_t bytesRead = 0;         // SourceRead
  aviotrix::Status status;
  // Synchronization
  std::mutex mutex;
  std::condition_variable cv;
  bool done = false;
};

struct LogMessage {
  std::string level;
  std::string text;
};

struct ProgressMessage {
  int64_t bytesRead;
  int64_t bytesWritten;
  std::optional<double> timestamp;
};

// Settles the Promise returned to JS for one operation. `json` is the payload on success.
struct Completion {
  std::shared_ptr<Napi::Promise::Deferred> deferred;
  aviotrix::Status status;
  std::string json;
  std::function<void()> afterSettle;  // runs on the main thread after resolve/reject (e.g. shutdown)
};

using Message = std::variant<IoRequest*, LogMessage, ProgressMessage, Completion>;

// Pumps Messages from the worker thread onto the JS main thread. Owned by NativeReader.
class MainThreadBridge {
 public:
  MainThreadBridge(Napi::Env env, Napi::Object host, const char* resourceName);
  ~MainThreadBridge();

  // Worker thread: blocks until the host method's Promise settles. Returns request.status.
  aviotrix::Status request(IoRequest& request);
  // Worker thread: fire-and-forget.
  void log(std::string level, std::string text);
  void progress(int64_t bytesRead, int64_t bytesWritten, std::optional<double> timestamp);
  void complete(Completion completion);

  // Main thread only.
  void keepAlive(bool on);  // Ref/Unref the TSFN so the event loop stays alive while an operation runs
  void release();           // after the worker thread has been joined

 private:
  struct Context {
    Napi::ObjectReference host;
    // Set by the TSFN finalizer. At environment teardown Node finalizes the TSFN before ObjectWrap
    // finalizers run, so release()/keepAlive() must not touch it afterwards.
    std::shared_ptr<bool> finalized;
  };
  static void callJs(Napi::Env env, Napi::Function, Context* ctx, Message* message);
  static void handleIo(Napi::Env env, Context* ctx, IoRequest* req);
  static void finish(IoRequest* req, aviotrix::Status status);
  static std::string errorMessage(Napi::Env env, Napi::Value err);

  using Tsfn = Napi::TypedThreadSafeFunction<Context, Message, &MainThreadBridge::callJs>;
  Napi::Env env_;  // main thread only: Ref/Unref need it and the TSFN does not expose one
  Context* context_;
  Tsfn tsfn_;
  std::shared_ptr<bool> finalized_;
  bool released_ = false;
};

}  // namespace aviotrix_node
