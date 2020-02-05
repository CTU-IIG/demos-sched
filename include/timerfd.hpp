#ifndef TIMERFD_HPP
#define TIMERFD_HPP

#include <ev++.h>
#include <chrono>
#include <functional>

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
    void set(std::function<void()> callback);
    void start(std::chrono::steady_clock::time_point timeout);
    ~timerfd();
private:
    std::function<void()> callback;
    void ev_callback(ev::io &w, int revents);

};
}

#endif
