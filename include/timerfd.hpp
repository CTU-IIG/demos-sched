#ifndef TIMERFD_HPP
#define TIMERFD_HPP

#include <ev++.h>
#include <chrono>

// std::chrono to timespec conversions
// https://embeddedartistry.com/blog/2019/01/31/converting-between-timespec-stdchrono/
constexpr timespec timepointToTimespec(
        std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> tp)
{
    auto secs = std::chrono::time_point_cast<std::chrono::seconds>(tp);
    auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(tp) -
            std::chrono::time_point_cast<std::chrono::nanoseconds>(secs);

    return timespec{secs.time_since_epoch().count(), ns.count()};
}

// ev wrapper around timerfd
namespace ev {
class timerfd : private io
{
public:
    timerfd();

    template<class K, void (K::*method)(timerfd &t)>
    void set (K *object) EV_NOEXCEPT
    {
        cb_this = object;
        cb_func = &callback_thunk<K, method>;
        io::set<timerfd, &timerfd::callback>(this);
    }

    void start (std::chrono::steady_clock::time_point timeout);
    ~timerfd();
private:
    void *cb_this;
    void (*cb_func)(timerfd &t, void *cb_this);

    void callback(ev::io &w, int revents);

    template<class K, void (K::*method)(timerfd &t)>
    static void callback_thunk (timerfd &t, void *callee_this)
    {
        (static_cast<K *>(callee_this)->*method)(t);
    }
};
}

#endif
