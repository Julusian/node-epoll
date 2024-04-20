#pragma once

#define NAPI_VERSION 8

#include <napi.h>

#include "watcher.h"

namespace epoll
{
  struct EpollInstanceData
  {
    std::weak_ptr<EpollWatcher> watcher;
    Napi::FunctionReference epollContructor;
  };

  class Epoll : public Napi::ObjectWrap<Epoll>
  {
  public:
    static Napi::FunctionReference Init(const Napi::Env &env, Napi::Object exports);

    Epoll(const Napi::CallbackInfo &info);
    ~Epoll();

    void DispatchEvent(const Napi::Env &env, int err, struct epoll_event *event);

  private:
    Napi::Value Add(const Napi::CallbackInfo &info);
    Napi::Value Modify(const Napi::CallbackInfo &info);
    Napi::Value Remove(const Napi::CallbackInfo &info);
    Napi::Value Close(const Napi::CallbackInfo &info);
    Napi::Value GetClosed(const Napi::CallbackInfo &info);

    Napi::FunctionReference callback_;
    Napi::AsyncContext async_context_;

    std::list<int> fds_;
    bool closed_;

    std::shared_ptr<EpollWatcher> watcher_;
  };
}
