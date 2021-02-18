#include "evfd.hpp"
#include "check_lib.hpp"
#include <stdint.h>
#include <sys/eventfd.h>
#include <unistd.h>

using namespace std;

ev::evfd::evfd(loop_ref loop)
    : io(loop)
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
        throw std::system_error(errno, std::generic_category(), std::string(__PRETTY_FUNCTION__));
}

void ev::evfd::set(std::function<void(ev::evfd &)> callback)
{
    this->callback = callback;
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
