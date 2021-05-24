#pragma once

#include "lib/file_lib.hpp"
#include <err.h>
#include <iostream>

/// Throw IOError with function name and stringified expr as
/// error message if expr evaluates to -1
#define CHECK(expr)                                                                                \
    ({                                                                                             \
        auto ret = (expr);                                                                         \
        if (ret == -1) throw IOError(std::string(__PRETTY_FUNCTION__) + ": " #expr);               \
        ret;                                                                                       \
    })

/// Throw IOError with given message if expr evaluates to -1
#define CHECK_MSG(expr, message)                                                                   \
    ({                                                                                             \
        auto ret = (expr);                                                                         \
        if (ret == -1) {                                                                           \
            throw IOError(message);                                                                \
        }                                                                                          \
        ret;                                                                                       \
    })
