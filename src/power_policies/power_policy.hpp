#pragma once
#include "power_manager.hpp"
#include "scheduler_events.hpp"

/**
 * Receives events from other parts of the scheduler and
 * changes CPU power states accordingly.
 */
class PowerPolicy : public SchedulerEvents
{};
