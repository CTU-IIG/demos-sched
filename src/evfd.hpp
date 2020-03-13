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
    void set(std::function<void()> callback);
    int get_fd();
    uint64_t get_value();
    using io::start;
    using io::stop;
    ~evfd();

private:
    std::function<void()> callback;
    void ev_callback(ev::io &w, int revents);
    uint64_t value;
};
}

#endif // EVFD_HPP
