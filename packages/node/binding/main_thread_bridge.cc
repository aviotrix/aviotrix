#include "main_thread_bridge.h"

#include <algorithm>
#include <vector>

namespace aviotrix_node {

using aviotrix::ErrorCode;
using aviotrix::Status;

MainThreadBridge::MainThreadBridge(Napi::Env env, Napi::Object host, const char* resourceName)
    : env_(env), finalized_(std::make_shared<std::atomic<bool>>(false)) {
  context_ = new Context{Napi::Persistent(host), finalized_};
  tsfn_ = Tsfn::New(env, resourceName, 0, 1, context_, [](Napi::Env, void*, Context* ctx) {
    *ctx->finalized = true;
    delete ctx;
  });
  tsfn_.Unref(env);  // idle readers must not keep the process alive
}

MainThreadBridge::~MainThreadBridge() {
  release();
}

void MainThreadBridge::release() {
  if (!usable()) return;
  released_ = true;
  tsfn_.Release();
}

void MainThreadBridge::keepAlive(bool on) {
  const int before = operationsInFlight_;
  operationsInFlight_ += on ? 1 : -1;
  if (!usable()) return;
  if (before == 0 && operationsInFlight_ == 1)
    tsfn_.Ref(env_);
  else if (before == 1 && operationsInFlight_ == 0)
    tsfn_.Unref(env_);
}

Status MainThreadBridge::request(IoRequest& req) {
  if (!usable()) return Status::Error(ErrorCode::IoFailed, "reader is shut down");
  req.done = false;
  Message* msg = new Message(&req);
  if (tsfn_.BlockingCall(msg) != napi_ok) {
    delete msg;
    return Status::Error(ErrorCode::IoFailed, "JS environment is shutting down");
  }
  std::unique_lock<std::mutex> lock(req.mutex);
  req.cv.wait(lock, [&] { return req.done; });
  return req.status;
}

void MainThreadBridge::log(std::string level, std::string text) {
  if (!usable()) return;
  Message* msg = new Message(LogMessage{std::move(level), std::move(text)});
  if (tsfn_.NonBlockingCall(msg) != napi_ok) delete msg;
}

void MainThreadBridge::progress(int64_t bytesRead, int64_t bytesWritten, std::optional<double> timestamp) {
  if (!usable()) return;
  Message* msg = new Message(ProgressMessage{bytesRead, bytesWritten, timestamp});
  if (tsfn_.NonBlockingCall(msg) != napi_ok) delete msg;
}

void MainThreadBridge::complete(Completion completion) {
  if (!usable()) return;
  Message* msg = new Message(std::move(completion));
  if (tsfn_.BlockingCall(msg) != napi_ok) delete msg;
}

void MainThreadBridge::finish(IoRequest* req, Status status) {
  // Notify while holding the lock: `req` and its cv live on the worker's stack, and once the worker can
  // observe `done` it may return from request() and destroy them.
  std::lock_guard<std::mutex> lock(req->mutex);
  req->status = std::move(status);
  req->done = true;
  req->cv.notify_one();
}

std::string MainThreadBridge::errorMessage(Napi::Env, Napi::Value err) {
  if (err.IsObject()) {
    Napi::Value m = err.As<Napi::Object>().Get("message");
    if (m.IsString()) return m.As<Napi::String>().Utf8Value();
  }
  return err.ToString().Utf8Value();
}

void MainThreadBridge::callJs(Napi::Env env, Napi::Function, Context* ctx, Message* message) {
  std::unique_ptr<Message> owned(message);
  if (!env) {  // environment torn down: unblock any waiter
    if (auto* req = std::get_if<IoRequest*>(message))
      finish(*req, Status::Error(ErrorCode::IoFailed, "environment shut down"));
    return;
  }
  Napi::HandleScope scope(env);
  Napi::Object host = ctx->host.Value();

  if (auto* req = std::get_if<IoRequest*>(message)) {
    handleIo(env, ctx, *req);
  } else if (auto* log = std::get_if<LogMessage>(message)) {
    // A throwing observer callback must not become an uncaught N-API callback exception.
    try {
      Napi::Value fn = host.Get("onLog");
      if (fn.IsFunction())
        fn.As<Napi::Function>().Call(host, {Napi::String::New(env, log->level), Napi::String::New(env, log->text)});
    } catch (const Napi::Error&) {
    }
  } else if (auto* p = std::get_if<ProgressMessage>(message)) {
    try {
      Napi::Value fn = host.Get("onProgress");
      if (fn.IsFunction()) {
        Napi::Value ts = p->timestamp ? Napi::Value(Napi::Number::New(env, *p->timestamp)) : Napi::Value(env.Null());
        fn.As<Napi::Function>().Call(host, {Napi::Number::New(env, static_cast<double>(p->bytesRead)),
                                            Napi::Number::New(env, static_cast<double>(p->bytesWritten)), ts});
      }
    } catch (const Napi::Error&) {
    }
  } else if (auto* c = std::get_if<Completion>(message)) {
    if (c->status.ok()) {
      c->deferred->Resolve(Napi::String::New(env, c->json));
    } else {
      Napi::Error err = Napi::Error::New(env, c->status.message);
      err.Set("code", Napi::String::New(env, aviotrix::errorCodeName(c->status.code)));
      c->deferred->Reject(err.Value());
    }
    if (c->afterSettle) c->afterSettle();
  }
}

void MainThreadBridge::handleIo(Napi::Env env, Context* ctx, IoRequest* req) {
  Napi::Object host = ctx->host.Value();
  const char* method = nullptr;
  std::vector<napi_value> args;
  switch (req->kind) {
    case IoRequest::Kind::SourceOpen:
      method = "sourceOpen";
      break;
    case IoRequest::Kind::SourceRead:
      method = "sourceRead";
      args = {Napi::Number::New(env, static_cast<double>(req->offset)),
              Napi::Number::New(env, static_cast<double>(req->readInto.size()))};
      break;
    case IoRequest::Kind::SourceClose:
      method = "sourceClose";
      break;
    case IoRequest::Kind::SinkOpen:
      method = "sinkOpen";
      break;
    case IoRequest::Kind::SinkWrite:
      method = "sinkWrite";
      args = {Napi::Number::New(env, static_cast<double>(req->offset)),
              Napi::Buffer<uint8_t>::Copy(env, req->writeFrom.data(), req->writeFrom.size())};
      break;
    case IoRequest::Kind::SinkClose:
      method = "sinkClose";
      break;
  }

  Napi::Value result;
  try {
    Napi::Value fn = host.Get(method);
    if (!fn.IsFunction()) {
      finish(req, Status::Error(ErrorCode::IoFailed, std::string("host has no method ") + method));
      return;
    }
    result = fn.As<Napi::Function>().Call(host, args);
  } catch (const Napi::Error& e) {
    finish(req, Status::Error(ErrorCode::IoFailed, errorMessage(env, e.Value())));
    return;
  }

  // Normalize sync values and Promises alike: Promise.resolve(result).then(onOk, onErr)
  Napi::Object promiseCtor = env.Global().Get("Promise").As<Napi::Object>();
  Napi::Value promise = promiseCtor.Get("resolve").As<Napi::Function>().Call(promiseCtor, {result});

  Napi::Function onOk = Napi::Function::New(env, [req](const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Value v = info[0];
    switch (req->kind) {
      case IoRequest::Kind::SourceOpen:
        if (v.IsNumber())
          req->size = static_cast<int64_t>(v.As<Napi::Number>().DoubleValue());
        else if (v.IsNull() || v.IsUndefined())
          req->size.reset();
        else {
          finish(req, Status::Error(ErrorCode::IoFailed, "sourceOpen must return a number or null"));
          return;
        }
        break;
      case IoRequest::Kind::SourceRead: {
        if (!v.IsTypedArray()) {
          finish(req, Status::Error(ErrorCode::IoFailed, "sourceRead must return a Uint8Array"));
          return;
        }
        Napi::TypedArray ta = v.As<Napi::TypedArray>();
        const uint8_t* data = static_cast<const uint8_t*>(ta.ArrayBuffer().Data()) + ta.ByteOffset();
        const size_t n = std::min(ta.ByteLength(), req->readInto.size());  // over-long reads are truncated
        std::copy_n(data, n, req->readInto.data());
        req->bytesRead = n;
        break;
      }
      default:
        break;
    }
    (void)env;
    finish(req, Status::Ok());
  });
  Napi::Function onErr = Napi::Function::New(env, [req](const Napi::CallbackInfo& info) {
    finish(req, Status::Error(ErrorCode::IoFailed, errorMessage(info.Env(), info[0])));
  });
  promise.As<Napi::Object>().Get("then").As<Napi::Function>().Call(promise, {onOk, onErr});
}

}  // namespace aviotrix_node
