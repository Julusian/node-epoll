#pragma once

#define NAPI_VERSION 8

#include <napi.h>

#include <thread>
#include <map>
#include <atomic>

namespace epoll
{
    class Epoll; // Declared later
    class EpollWatcher;

    struct DataType
    {
        struct epoll_event event;
        int error;
    };

    using Context = EpollWatcher; // Napi::Reference<Napi::Value>;
    // using DataType = struct epoll_event;
    void CallJs(Napi::Env env, Napi::Function callback, Context *context, DataType *data);
    using TSFN = Napi::TypedThreadSafeFunction<Context, DataType, CallJs>;
    using FinalizerDataType = void;

    class EpollWatcher
    {
    public:
        EpollWatcher(const Napi::Env &env);
        ~EpollWatcher();

        int Add(int fd, uint32_t events, Epoll *epoll);
        int Modify(int fd, uint32_t events);
        int Remove(int fd);

        void HandleEvent(const Napi::Env &env, DataType *event);

    private:
        void Cleanup();

        int epfd;

        std::thread nativeThread;
        TSFN tsfn;

        std::map<int, Epoll *> fd2epoll;

        std::atomic<bool> abort_ = {false};
    };
}