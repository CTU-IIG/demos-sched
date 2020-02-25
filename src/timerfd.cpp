#include "timerfd.hpp"
#include "demossched.hpp"
#include <err.h>
#include <iostream>
#include <sys/timerfd.h>
#include <unistd.h>

ev::timerfd::timerfd(loop_ref loop)
    : io(loop)
{
    // create timer
    // steady_clock == CLOCK_MONOTONIC
    int fd = CHECK(timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC));
    io::set(fd, ev::READ);
    io::set<timerfd, &timerfd::ev_callback>(this);
}

void ev::timerfd::set(std::function<void()> callback)
{
    this->callback = callback;
}

void ev::timerfd::start(std::chrono::steady_clock::time_point timeout)
{
    // set timeout
    struct itimerspec timer_value;
    // first launch
    timer_value.it_value = timepointToTimespec(timeout);
    // no periodic timer
    timer_value.it_interval = timespec{ 0, 0 };

    CHECK(timerfd_settime(fd, TFD_TIMER_ABSTIME, &timer_value, NULL));

    io::start();
}

ev::timerfd::~timerfd()
{
    stop();
    close(fd);
}

void ev::timerfd::ev_callback(ev::io &w, int revents)
{
    if (EV_ERROR & revents)
        err(1, "ev cb: got invalid event");

    // std::cout << "timeout " << std::endl;
    // read to have empty fd
    uint64_t buf;
    CHECK(::read(w.fd, &buf, sizeof(buf)));
    w.stop(); // Is this necessary?
    callback();
}
