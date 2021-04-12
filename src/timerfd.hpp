#ifndef TIMERFD_HPP
#define TIMERFD_HPP

#include <chrono>
#include <ev++.h>
#include <functional>

// std::chrono to timespec conversions
// https://embeddedartistry.com/blog/2019/01/31/converting-between-timespec-stdchrono/
constexpr timespec timepointToTimespec(
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> tp)
{
    using namespace std::chrono;
    auto secs = time_point_cast<seconds>(tp);
    auto ns = time_point_cast<nanoseconds>(tp) - time_point_cast<nanoseconds>(secs);

    return timespec{ secs.time_since_epoch().count(), ns.count() };
}

// ev wrapper around timerfd
namespace ev {
class timerfd : private io
{
public:
    explicit timerfd(ev::loop_ref loop);
    ~timerfd();
    void set(std::function<void()> callback);
    void start(std::chrono::steady_clock::time_point timeout);
    using io::priority;
    using io::stop;

private:
    std::function<void()> callback = nullptr;
    void ev_callback(ev::io &w, int revents);
};
}

#endif
