#pragma once

#include <ev++.h>
#include <functional>

// ev wrapper around eventfd
namespace ev {
class evfd : private io
{
public:
    explicit evfd(ev::loop_ref loop);
    void set(std::function<void(ev::evfd &)> callback_);
    int get_fd();
    using io::start;
    using io::stop;
    ~evfd();
    void write(uint64_t val);

private:
    std::function<void(ev::evfd &)> callback = nullptr;
    void ev_callback(ev::io &w, int revents);
};
}
