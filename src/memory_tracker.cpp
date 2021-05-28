#include "memory_tracker.hpp"
#include "config_meson.h"

#ifndef LOG_ALLOCATIONS
void memory_tracker::enable() {}
void memory_tracker::disable() {}
#else

#include "log.hpp"
#include <cstdlib>
#include <new>

static bool log_allocations = false;

void memory_tracker::enable()
{
    if (!log_allocations) {
        log_allocations = true;
        logger->info("Heap allocation logging enabled");
    }
}

void memory_tracker::disable()
{
    if (log_allocations) {
        log_allocations = false;
        logger->info("Heap allocation logging disabled");
    }
}

void *operator new(size_t size)
{
    if (log_allocations) {
        logger->warn("Allocating {} bytes", size);
    }
    void *p = malloc(size);
    if (p == nullptr) throw std::bad_alloc();
    return p;
}

void operator delete(void *p) noexcept
{
    free(p);
}

#endif
