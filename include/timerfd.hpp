#ifndef TIMERFD_HPP
#define TIMERFD_HPP

#include <ev++.h>
#include <chrono>
#include "functions.hpp"

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
namespace ev{
    class timerfd : public io
    {
        private:
            int timer_fd;
        public:
            timerfd() : io()
        {
            // create timer
            // steady_clock == CLOCK_MONOTONIC
            timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
            if( timer_fd < 0 )
                err(1,"timerfd_create");
        }

            void start (std::chrono::steady_clock::time_point timeout)
            {
                // set timeout
                struct itimerspec timer_value;
                // first launch
                timer_value.it_value = timepointToTimespec(timeout);
                // no periodic timer
                timer_value.it_interval = timespec{0,0};

                if( timerfd_settime( timer_fd, TFD_TIMER_ABSTIME, &timer_value, NULL) == -1 )
                    err(1,"timerfd_settime");

                // set fd to callback
                io::start(timer_fd, ev::READ);
            }
            ~timerfd()
            {
                close(timer_fd);
            }
    };
}

#endif
