#ifndef EVFD_HPP
#define EVFD_HPP

#include <ev++.h>
#include <functional>

// ev wrapper around eventfd
namespace ev {
class evfd : private io
{
public:
    evfd(loop_ref loop, int fd);
    evfd(ev::loop_ref loop);
    void set(std::function<void(ev::evfd*, uint64_t)> callback);
    int get_fd();
    using io::start;
    using io::stop;
    ~evfd();
    void write(const uint64_t val);

private:
    std::function<void(ev::evfd*, uint64_t)> callback;
    void ev_callback(ev::io &w, int revents);
};
}

#endif // EVFD_HPP
