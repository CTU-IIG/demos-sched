#include "evfd.hpp"
#include "demossched.hpp"
#include <sys/eventfd.h>
#include <unistd.h>
#include <stdint.h>

using namespace std;

ev::evfd::evfd(loop_ref loop)
    : io(loop)
{
    // create efd
    int fd = CHECK(eventfd(0, EFD_NONBLOCK));

    io::set(fd, ev::READ);
    io::set<evfd, &evfd::ev_callback>(this);
}

ev::evfd::~evfd()
{
    close(fd);
}

void ev::evfd::set(std::function<void()> callback)
{
    this->callback = callback;
}

int ev::evfd::get_fd()
{
    return fd;
}

uint64_t ev::evfd::get_value()
{
    return value;
}

void ev::evfd::ev_callback(ev::io &w, int revents)
{
    if (EV_ERROR & revents)
        err(1, "ev cb: got invalid event");

    //cout <<__PRETTY_FUNCTION__<< endl;
    // read to have empty fd
    CHECK(::read(w.fd, &value, sizeof(value)));
    w.stop(); // Is this necessary?
    callback();
}
