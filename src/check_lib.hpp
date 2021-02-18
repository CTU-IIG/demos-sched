#pragma once

#include <err.h>
#include <iostream>

/// Throw std::system_error with function name and stringified expr as
/// error message if expr evaluates to -1
#define CHECK(expr)                                                                                \
    ({                                                                                             \
        auto ret = (expr);                                                                         \
        if (ret == -1)                                                                             \
            throw std::system_error(                                                               \
              errno, std::generic_category(), std::string(__PRETTY_FUNCTION__) + ": " #expr);      \
        ret;                                                                                       \
    })

/// Throw std::system_error with given message if expr evaluates to -1
#define CHECK_MSG(expr, message)                                                                   \
    ({                                                                                             \
        auto ret = (expr);                                                                         \
        if (ret == -1) {                                                                           \
            throw std::system_error(errno, std::generic_category(), message);                      \
        }                                                                                          \
        ret;                                                                                       \
    })
