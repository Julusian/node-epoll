#ifdef __linux__

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <list>
#include "watcher.h"
#include "epoll.h"

namespace epoll
{
    // Transform native data into JS data, passing it to the provided
    // `callback` -- the TSFN's JavaScript function.
    void CallJs(Napi::Env env, Napi::Function callback, Context *context,
                DataType *data)
    {

        // Is the JavaScript environment still available to call into, eg. the TSFN is
        // not aborted
        if (env != nullptr && context != nullptr && data != nullptr)
        {
            // This method is executed in the event loop thread.
            // By the time flow of control arrives here the original Epoll instance that
            // registered interest in the event may no longer have this interest. If
            // this is the case, the event will be silently ignored.

            std::map<int, Epoll *>::iterator it = context->fd2epoll.find(data->event.data.fd);
            if (it != context->fd2epoll.end())
            {
                it->second->DispatchEvent(env, data->error, &data->event);
            }
        }

        if (data != nullptr)
        {
            // We're finished with the data.
            delete data;
        }
    }

    /*
     * Epoll
     */

    EpollWatcher::EpollWatcher(const Napi::Env &env)
    {

        auto epfd = epoll_create1(0);
        if (epfd == -1)
        {
            Napi::Error::New(env, strerror(errno)).ThrowAsJavaScriptException();
            return;
        }

        // Create a context that can be 'leaked' to the native thread, and cleaned up when the tsfn is destroyed
        auto context = new WatcherContext;
        this->context = context;

        context->epfd = epfd;

        // Create a native thread
        context->nativeThread = std::thread([context]
                                            {

                                      int count;

                                      struct DataType *data = new DataType;

                                      while (!context->abort_)
                                      {
                                          count = epoll_wait(context->epfd, &data->event, 1, 50);
                                          if (context->abort_)
                                              break;

                                          if (count == 0 || (count == -1 && errno == EINTR))
                                              continue;

                                          data->error = count == -1 ? errno : 0;

                                          // Block until the event loop has handled the call, to ensure there isn't a long queue for processing
                                          // Old code said:
                                          // Wait till the event loop says it's ok to poll. The semaphore serves more
                                          // than one purpose.
                                          // - When level-triggered epoll is used, the default when EPOLLET isn't
                                          //   specified, the event triggered by the last call to epoll_wait may be
                                          //   trigged again and again if the condition that triggered it hasn't been
                                          //   cleared yet. Waiting prevents multiple triggers for the same event.
                                          // - It forces a context switch from the watcher thread to the event loop
                                          //   thread.
                                          napi_status status = context->tsfn.BlockingCall(data);

                                          data = new DataType;
                                          if (status != napi_ok)
                                          {
                                              // Ignore error
                                          }
                                      }

                                      delete data;

                                      // Release the thread-safe function
                                      context->tsfn.Release(); });

        // Create a ThreadSafeFunction
        context->tsfn = TSFN::New(
            env,
            // callback,               // JavaScript function called asynchronously
            "Epoll:DispatchEvent", // Name
            1,                     // Queue size
            1,                     // Only one thread will use this initially
            context,               // context,
            [&](Napi::Env, FinalizerDataType *,
                Context *ctx) { // Finalizer used to clean threads up
                if (ctx->nativeThread.joinable())
                {
                    ctx->nativeThread.join();
                }
                delete ctx;
            });
    };

    EpollWatcher::~EpollWatcher()
    {
        // The destructor is not guaranteed to be called, but we can perform the same cleanup which gets triggered manually elsewhere
        Cleanup();
    };

    void EpollWatcher::Cleanup()
    {
        if (context->epfd != -1)
        {
            close(context->epfd);
            context->epfd = -1;
        }

        context->abort_ = true;

        context = nullptr;
    }

    int EpollWatcher::Add(int fd, uint32_t events, Epoll *epoll)
    {
        if (context == nullptr)
            return 111;

        if (context->fd2epoll.count(fd) > 0)
        {
            // Already being watched somewhere
            return 111;
        }

        struct epoll_event event;
        event.events = events;
        event.data.fd = fd;

        if (epoll_ctl(context->epfd, EPOLL_CTL_ADD, fd, &event) == -1)
            return errno;

        context->fd2epoll.insert(std::pair<int, Epoll *>(fd, epoll));

        return 0;
    }

    int EpollWatcher::Modify(int fd, uint32_t events)
    {
        if (context == nullptr)
            return 111;

        struct epoll_event event;
        event.events = events;
        event.data.fd = fd;

        if (epoll_ctl(context->epfd, EPOLL_CTL_MOD, fd, &event) == -1)
            return errno;

        return 0;
    }

    int EpollWatcher::Remove(int fd)
    {
        if (context == nullptr)
            return 111;

        if (epoll_ctl(context->epfd, EPOLL_CTL_DEL, fd, 0) == -1)
            return errno;

        context->fd2epoll.erase(fd);

        return 0;
    }

    void EpollWatcher::Forget(Epoll *epoll)
    {
        if (context == nullptr)
            return;

        for (std::map<int, Epoll *>::iterator it = context->fd2epoll.begin(); it != context->fd2epoll.end();)
        {
            if (it->second == epoll)
            {
                epoll_ctl(context->epfd, EPOLL_CTL_DEL, it->first, 0);
                it = context->fd2epoll.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

}
#endif