/**
 * This module logs all heap allocations (using `new` operator override) when enabled.
 */
#pragma once

namespace memory_tracker {
void enable();
void disable();
}
