/**
 * Version of `assert` that throws runtime_error instead.
 *
 * This allows the stack to unwind gracefully, so that DEmOS doesn't
 * leave cgroups and other resources behind on a failed assertion.
 */
#pragma once
#include <log.hpp>
#include <stdexcept>

class assertion_error : public std::runtime_error
{
public:
    explicit assertion_error(const std::string &expression,
                             const std::string &src_file,
                             size_t src_line,
                             const std::string &fn_name)
        : std::runtime_error("Assertion failed (see above)")
    {
        using std::string_literals::operator""s;
        logger->critical("Assertion failed: `" + expression + "`");
        logger->critical("    at "s + src_file + ":" + std::to_string(src_line) + " (" + fn_name +
                         ")");
        logger->critical("Unwinding stack and exiting...");
    }
};

// TODO: use `if constexpr` instead of macro trickery
#ifdef DEBUG
#define ASSERT(expr)                                                                               \
    (static_cast<bool>(expr)                                                                       \
       ? void(0)                                                                                   \
       : throw assertion_error(#expr, __FILE__, __LINE__, __PRETTY_FUNCTION__))
#define RUN_DEBUG(expr) void(expr)
#define IF_DEBUG(yes, no) (yes)
#else
// type-check the assert expressions even in release mode, but don't run them
//  (compiler should optimize this away into a no-op)
// the inner (void)(expr) suppresses the "expression result unused" warning
#define RUN_DEBUG(expr) void(true || ((void)(expr), false))
#define ASSERT(expr) RUN_DEBUG(expr)
#define IF_DEBUG(yes, no) (no)
#endif
