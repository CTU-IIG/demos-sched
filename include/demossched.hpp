#ifndef DEMOSSCHED_HPP
#define DEMOSSCHED_HPP

#include <iostream>
#include <err.h>

#define CHECK(expr) ({ auto ret = (expr); if (ret == -1) throw std::system_error(errno, std::generic_category(), std::string(__PRETTY_FUNCTION__) + ": " #expr); ret; })

#define VERBOSE

#endif // DEMOSSCHED_HPP
