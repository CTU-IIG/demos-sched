#include "memory_tracker.hpp"
#include "config_meson.h"

#ifndef LOG_ALLOCATIONS
void enable_allocation_logging() {}
void disable_allocation_logging() {}
#else

#include "log.hpp"
#include <cstdlib>
#include <new>

static bool log_allocations = false;

void enable_allocation_logging()
{
    log_allocations = true;
    logger->info("Heap allocation logging enabled");
}

void disable_allocation_logging()
{
    log_allocations = false;
    logger->info("Heap allocation logging disabled");
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
