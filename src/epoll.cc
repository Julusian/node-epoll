#ifdef __linux__

#include <errno.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <list>
#include "epoll.h"

#include <iostream>

namespace epoll
{
  // TODO - strerror isn't threadsafe, use strerror_r instead
  // TODO - use uv_strerror rather than strerror_r for libuv errors?

  Epoll::Epoll(const Napi::CallbackInfo &info)
      : Napi::ObjectWrap<Epoll>(info),
        async_context_(Napi::AsyncContext(info.Env(), "Epoll")),
        closed_(false)
  {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsFunction())
    {
      Napi::Error::New(env, "First argument to construtor must be a callback").ThrowAsJavaScriptException();
      return;
    }

    callback_ = Napi::Persistent(info[0].As<Napi::Function>());
  };

  Epoll::~Epoll()
  {
    // The destructor is not guaranteed to be called, but we can perform the same cleanup which gets triggered manually elsewhere

    watcher_ = nullptr;
  };

  Napi::FunctionReference
  Epoll::Init(const Napi::Env &env, Napi::Object exports)
  {
    // This method is used to hook the accessor and method callbacks
    Napi::Function func = DefineClass(env, "Epoll", {

                                                        InstanceMethod<&Epoll::Add>("add", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                                        //
                                                        InstanceMethod<&Epoll::Close>("close", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                                        //
                                                        InstanceMethod<&Epoll::Remove>("remove", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                                        //
                                                        InstanceMethod<&Epoll::Modify>("modify", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                                        //
                                                        InstanceAccessor<&Epoll::GetClosed>("closed", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                                        // StaticMethod<&Example::CreateNewItem>("CreateNewItem", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),

                                                        StaticValue("EPOLLIN", Napi::Number::New(env, EPOLLIN), napi_default),
                                                        StaticValue("EPOLLOUT", Napi::Number::New(env, EPOLLOUT), napi_default),
                                                        StaticValue("EPOLLRDHUP", Napi::Number::New(env, EPOLLRDHUP), napi_default),
                                                        StaticValue("EPOLLPRI", Napi::Number::New(env, EPOLLPRI), napi_default),
                                                        StaticValue("EPOLLERR", Napi::Number::New(env, EPOLLERR), napi_default),
                                                        StaticValue("EPOLLHUP", Napi::Number::New(env, EPOLLHUP), napi_default),
                                                        StaticValue("EPOLLET", Napi::Number::New(env, EPOLLET), napi_default),
                                                        StaticValue("EPOLLONESHOT", Napi::Number::New(env, EPOLLONESHOT), napi_default),
                                                    });

    exports.Set("Epoll", func);

    return Napi::Persistent(func);
  }

  Napi::Value Epoll::Add(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();

    if (this->closed_)
    {
      Napi::Error::New(env, "add can't be called after calling close").ThrowAsJavaScriptException();
      return env.Null();
    }

    // Epoll.EPOLLET is -0x8000000 on ARM and an IsUint32 check fails so
    // check for IsNumber instead.
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber())
    {
      Napi::Error::New(env, "incorrect arguments passed to add"
                            "(int fd, int events)")
          .ThrowAsJavaScriptException();
      return env.Null();
    }

    int fd = info[0].As<Napi::Number>().Int32Value();
    int events = info[1].As<Napi::Number>().Int32Value();

    // Take a reference or create the watcher
    if (!watcher_)
    {
      auto data = env.GetInstanceData<EpollInstanceData>();
      if (!data)
      {
        Napi::Error::New(env, "Library is not initialised correctly").ThrowAsJavaScriptException();
        return env.Null();
      }

      watcher_ = data->watcher.lock();
      if (!watcher_)
      {
        watcher_ = std::make_shared<EpollWatcher>(env);
        data->watcher = watcher_;
      }
    }

    int err = watcher_->Add(fd, events, this);
    if (err != 0)
    {
      Napi::Error::New(env, strerror(err)).ThrowAsJavaScriptException();
      return env.Null(); // TODO - use err also
    }

    fds_.push_back(fd);

    return info.This();
  }

  Napi::Value Epoll::Modify(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();

    if (this->closed_)
    {
      Napi::Error::New(env, "modify can't be called after calling close").ThrowAsJavaScriptException();
      return env.Null();
    }

    // Epoll.EPOLLET is -0x8000000 on ARM and an IsUint32 check fails so
    // check for IsNumber instead.
    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber())
    {
      Napi::Error::New(env, "incorrect arguments passed to modify"
                            "(int fd, int events)")
          .ThrowAsJavaScriptException();
      return env.Null();
    }

    if (!watcher_)
    {
      Napi::Error::New(env, "modify can't be called when not watching a fd").ThrowAsJavaScriptException();
      return env.Null();
    }

    // TODO - validate this is watching fd

    int err = watcher_->Modify(
        info[0].As<Napi::Number>().Int32Value(),
        info[1].As<Napi::Number>().Int32Value());
    if (err != 0)
    {
      Napi::Error::New(env, strerror(err)).ThrowAsJavaScriptException();
      return env.Null(); // TODO - use err also
    }

    return info.This();
  }

  Napi::Value Epoll::Remove(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();

    if (this->closed_)
    {
      Napi::Error::New(env, "remove can't be called after calling close").ThrowAsJavaScriptException();
      return env.Null();
    }

    if (info.Length() < 1 || !info[0].IsNumber())
    {
      Napi::Error::New(env, "incorrect arguments passed to remove(int fd)").ThrowAsJavaScriptException();
      return env.Null();
    }

    int fd = info[0].As<Napi::Number>().Int32Value();

    int err = watcher_->Remove(fd);

    fds_.remove(fd);
    if (fds_.empty())
    {
      watcher_ = nullptr;
    }

    if (err != 0)
    {
      Napi::Error::New(env, strerror(err)).ThrowAsJavaScriptException();
      return env.Null();
    }

    return info.This();
  }

  Napi::Value Epoll::Close(const Napi::CallbackInfo &info)
  {
    Napi::Env env = info.Env();

    if (this->closed_)
    {
      Napi::Error::New(env, "close can't be called more than once").ThrowAsJavaScriptException();
      return env.Null();
    }

    closed_ = true;

    int error = 0;

    for (int fd : fds_)
    {
      int err = watcher_->Remove(fd);
      if (err != 0)
        error = err; // TODO - This will only return one of many errors
    }

    watcher_ = nullptr;

    if (error != 0)
    {
      Napi::Error::New(env, strerror(error)).ThrowAsJavaScriptException();
      return env.Null(); // TODO - use err also
    }

    return env.Null();
  }

  Napi::Value Epoll::GetClosed(const Napi::CallbackInfo &info)
  {
    return Napi::Boolean::New(info.Env(), this->closed_);
  }

  void Epoll::DispatchEvent(const Napi::Env &env, int err, struct epoll_event *event)
  {
    Napi::HandleScope scope(env);

    try
    {
      if (err)
      {
        callback_.MakeCallback(Value(), std::initializer_list<napi_value>{Napi::Error::New(env, strerror(err)).Value()}, async_context_);
      }
      else
      {
        callback_.MakeCallback(Value(), std::initializer_list<napi_value>{env.Null(), Napi::Number::New(env, event->data.fd), Napi::Number::New(env, event->events)},
                               async_context_);
      }
    }
    catch (...)
    {
      // TODO - what to do with this error?
    }
  }

  Napi::Object Init(Napi::Env env, Napi::Object exports)
  {
    auto instanceData = new EpollInstanceData;

    instanceData->epollContructor = Epoll::Init(env, exports);

    // Store the constructor as the add-on instance data. This will allow this
    // add-on to support multiple instances of itself running on multiple worker
    // threads, as well as multiple instances of itself running in different
    // contexts on the same thread.
    //
    // By default, the value set on the environment here will be destroyed when
    // the add-on is unloaded using the `delete` operator, but it is also
    // possible to supply a custom deleter.
    env.SetInstanceData<EpollInstanceData>(instanceData);

    return exports;
  }

  NODE_API_MODULE(epoll, Init)
}

#endif
