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

    struct WatcherContext;

    using Context = WatcherContext; // Napi::Reference<Napi::Value>;
    // using DataType = struct epoll_event;
    void CallJs(Napi::Env env, Napi::Function callback, Context *context, DataType *data);
    using TSFN = Napi::TypedThreadSafeFunction<Context, DataType, CallJs>;
    using FinalizerDataType = void;

    struct WatcherContext
    {
        std::atomic<bool> abort_ = {false};

        int epfd;
        std::map<int, Epoll *> fd2epoll;

        std::thread nativeThread;
        TSFN tsfn;
    };

    class EpollWatcher : public std::enable_shared_from_this<EpollWatcher>
    {
    public:
        EpollWatcher(const Napi::Env &env);
        ~EpollWatcher();

        int Add(int fd, uint32_t events, Epoll *epoll);
        int Modify(int fd, uint32_t events);
        int Remove(int fd);
        void Forget(Epoll *epoll);

        void HandleEvent(const Napi::Env &env, DataType *event);

    private:
        void Cleanup();

        WatcherContext *context;
    };
}