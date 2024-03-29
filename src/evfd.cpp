#include "evfd.hpp"
#include "lib/check_lib.hpp"
#include <cstdint>
#include <sys/eventfd.h>
#include <unistd.h>

using namespace std;

ev::evfd::evfd(loop_ref loop_)
    : io(loop_)
{
    fd = CHECK(eventfd(0, EFD_NONBLOCK));
    io::set(fd, ev::READ);
    io::set<evfd, &evfd::ev_callback>(this);
}

ev::evfd::~evfd()
{
    close(fd);
}

void ev::evfd::write(const uint64_t val)
{
    if (::write(fd, &val, sizeof(uint64_t)) == -1)
        throw IOError(std::string(__PRETTY_FUNCTION__));
}

void ev::evfd::set(std::function<void(ev::evfd &)> callback_)
{
    this->callback = std::move(callback_);
}

int ev::evfd::get_fd()
{
    return fd;
}

void ev::evfd::ev_callback(ev::io &w, int revents)
{
    if (EV_ERROR & revents) err(1, "ev cb: got invalid event");

    // cout <<__PRETTY_FUNCTION__<< endl;
    // read to have empty fd
    uint64_t val;
    CHECK(::read(w.fd, &val, sizeof(val)));
    callback(*this);
}
