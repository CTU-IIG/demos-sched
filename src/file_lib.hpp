#pragma once

#include <err.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

class IOError : public std::runtime_error
{
private:
    const int _errno_;

public:
    explicit IOError(int errno_, const std::string &msg)
        : std::runtime_error(msg + ": errno `" + std::to_string(errno_) + "` - `" +
                             strerror(errno_) + "`")
        , _errno_(errno_)
    {}

    [[nodiscard]] int errno_() const {
        return _errno_;
    }

    [[nodiscard]] std::string err_str() const {
        return std::string(strerror(_errno_));
    }
};

template<typename T>
inline T file_open(const std::filesystem::path &path, bool enable_exceptions = true)
{
    // technically, this is a hack, as it is not defined in the standard that fstream operations
    //  set an errno; but honestly, the C++ streams API intentionally encourages hacks,
    //  as I have no idea how else could it be this bad otherwise
    T stream(path);
    if (stream.fail()) {
        throw IOError(errno, "Failed to open file `" + path.string() + "`");
    }
    if (enable_exceptions) {
        stream.exceptions(T::badbit | T::failbit);
    }
    return stream;
}

template<typename T>
inline void file_open(T &stream, const std::filesystem::path &path, bool enable_exceptions = true)
{
    stream.open(path);
    if (stream.fail()) {
        throw IOError(errno, "Failed to open file `" + path.string() + "`");
    }
    if (enable_exceptions) {
        stream.exceptions(T::badbit | T::failbit);
    }
}
