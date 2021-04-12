/**
 * This module logs all heap allocations (using `new` operator override) when enabled.
 */
#pragma once

void enable_allocation_logging();
void disable_allocation_logging();
