#pragma once

#include <err.h>
#include <filesystem>
#include <fstream>
#include <iostream>

template <typename T>
inline T file_open(const std::filesystem::path &path, bool enable_exceptions = true)
{
    // technically, this is a hack, as it is not defined in the standard that fstream operations
    //  set an errno; but honestly, the C++ streams API intentionally encourages hacks,
    //  as I have no idea how else could it be this bad otherwise
    T stream(path);
    if (stream.fail()) {
        throw std::runtime_error("Failed to open file `" + path.string() + "`: errno `" +
                            std::to_string(errno) + "` - `" + strerror(errno) + "`");
    }
    if (enable_exceptions) {
        stream.exceptions(T::badbit | T::failbit);
    }
    return stream;
}

template <typename T>
inline void file_open(T& stream, const std::filesystem::path &path, bool enable_exceptions = true)
{
    stream.open(path);
    if (stream.fail()) {
        throw std::runtime_error("Failed to open file `" + path.string() + "`: errno `" +
                                 std::to_string(errno) + "` - `" + strerror(errno) + "`");
    }
    if (enable_exceptions) {
        stream.exceptions(T::badbit | T::failbit);
    }
}
