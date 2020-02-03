#include <unistd.h>
#include "timerfd.hpp"
#include <sys/timerfd.h>
#include <err.h>
#include <iostream>

ev::timerfd::timerfd() : io()
{
    // create timer
    // steady_clock == CLOCK_MONOTONIC
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if( fd < 0 )
        err(1,"timerfd_create");
    io::set(fd, ev::READ);
}

void ev::timerfd::start(std::chrono::steady_clock::time_point timeout)
{
    // set timeout
    struct itimerspec timer_value;
    // first launch
    timer_value.it_value = timepointToTimespec(timeout);
    // no periodic timer
    timer_value.it_interval = timespec{0,0};

    if( timerfd_settime(fd, TFD_TIMER_ABSTIME, &timer_value, NULL) == -1 )
        err(1,"timerfd_settime");

    // set fd to callback
    io::start();
}

ev::timerfd::~timerfd()
{
    stop();
    close(fd);
}

void ev::timerfd::callback(ev::io &w, int revents) {
    if (EV_ERROR & revents)
        err(1,"ev cb: got invalid event");

    std::cout << "timeout " << std::endl;
    // read to have empty fd
    uint64_t buf;
    int ret = ::read(w.fd, &buf, 10);
    if(ret != sizeof(uint64_t) )
        err(1, "read timerfd");
    cb_func(*this, cb_this);
    w.stop();

}
