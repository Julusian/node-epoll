#ifdef __linux__

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <list>
#include "watcher.h"
#include "epoll.h"

#include <iostream>

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
            context->HandleEvent(env, data);
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
        epfd = epoll_create1(0);
        if (epfd == -1)
        {
            // TODO - error
            // Napi::Error::New(info.Env(), strerror(errno)).ThrowAsJavaScriptException();
            return;
        }

        // Create a new context set to the receiver (ie, `this`) of the function call
        // Context *context = new Reference<Napi::Value>(Persistent(info.This()));

        // TODO - this should move so that it is shared across all instances of this class!

        // Create a ThreadSafeFunction
        this->tsfn = TSFN::New(
            env,
            // callback,               // JavaScript function called asynchronously
            "Epoll:DispatchEvent", // Name
            1,                     // Unlimited queue
            1,                     // Only one thread will use this initially
            this,                  // context,
            [&](Napi::Env, FinalizerDataType *,
                Context *ctx) { // Finalizer used to clean threads up
                if (nativeThread.joinable())
                {
                    nativeThread.join(); // TODO?
                }
                // delete ctx;
            });

        // Create a native thread
        this->nativeThread = std::thread([&]
                                         {

                                      int count;

                                      struct DataType *data = new DataType;

                                      while (!abort_) // TODO - when does this terminate?
                                      {
                                        //   do
                                        //   {
                                              count = epoll_wait(epfd, &data->event, 1, 50);
                                        //   } while (!abort_ && (count == 0 || (count == -1 && errno == EINTR)));
                                              if (abort_)
                                                  break;
                                             
                                              if (count == 0 || (count == -1 && errno == EINTR))
                                                  continue;

                                             

                                          data->error = count == -1 ? errno : 0;

                                            std::cout << "do thing" << std::endl;
                                            
                                            // TODO - this can deadlock if process.exit is called inside of this BlockingCall.
                                            // We need to be able to emit the thing in such a way that 

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
                                          napi_status status = tsfn.BlockingCall(data);

                                          std::cout << "done thing" << std::endl;
                                          data = new DataType;
                                          if (status != napi_ok)
                                          {
                                              // Ignore error
                                          }
                                      }

                                      std::cout << "term" << std::endl;
                                      delete data;

                                      // Release the thread-safe function
                                      tsfn.Release(); });
    };

    EpollWatcher::~EpollWatcher()
    {
        // The destructor is not guaranteed to be called, but we can perform the same cleanup which gets triggered manually elsewhere
        Cleanup();
    };

    void EpollWatcher::Cleanup()
    {
        abort_ = true;

        if (epfd != -1)
        {
            close(epfd);
            epfd = -1;
        }

        std::cout << "stopping" << std::endl;

        if (nativeThread.joinable())
            nativeThread.join();
    }

    int EpollWatcher::Add(int fd, uint32_t events, Epoll *epoll)
    {
        if (fd2epoll.count(fd) > 0)
        {
            // Already being watched somewhere
            return 111;
        }

        struct epoll_event event;
        event.events = events;
        event.data.fd = fd;

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) == -1)
            return errno;

        fd2epoll.insert(std::pair<int, Epoll *>(fd, epoll));

        return 0;
    }

    int EpollWatcher::Modify(int fd, uint32_t events)
    {
        struct epoll_event event;
        event.events = events;
        event.data.fd = fd;

        if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event) == -1)
            return errno;

        return 0;
    }

    int EpollWatcher::Remove(int fd)
    {
        if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, 0) == -1)
            return errno;

        fd2epoll.erase(fd);

        return 0;
    }

    void EpollWatcher::HandleEvent(const Napi::Env &env, DataType *event)
    {
        // This method is executed in the event loop thread.
        // By the time flow of control arrives here the original Epoll instance that
        // registered interest in the event may no longer have this interest. If
        // this is the case, the event will be silently ignored.

        std::map<int, Epoll *>::iterator it = fd2epoll.find(event->event.data.fd);
        if (it != fd2epoll.end())
        {
            it->second->DispatchEvent(env, event->error, &event->event);
        }
    }
}
#endif