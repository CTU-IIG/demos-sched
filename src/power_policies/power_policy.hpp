#pragma once

/**
 * Receives events from other parts of the scheduler and
 * changes CPU power states accordingly.
 */
class PowerPolicy : public SchedulerEvents
{
protected:
    PowerManager pm{};
};
