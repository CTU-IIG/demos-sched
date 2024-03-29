#include "slice.hpp"
#include "log.hpp"
#include "power_policy/_power_policy.hpp"
#include <lib/assert.hpp>
#include <fstream>

time_point Slice::start_time;

static class ScheduleLogger
{
public:
    ScheduleLogger()
    {
        const char *log = getenv("DEMOS_SCHEDULE_LOG");
        if (!log) return;
        if constexpr (enabled) {
            schedule_out.open(log, std::ios_base::trunc);
        } else {
            throw runtime_error(
              "DEMOS_SCHEDULE_LOG env. variable is not supported in release build");
        }
    }
    void log(const std::string &msg)
    {
        if constexpr (enabled) schedule_out << msg;
    }

private:
    static constexpr bool enabled = IF_DEBUG(true, false);
    std::ofstream schedule_out{};
} schedule_logger;

/*
TODO: I think there is a subtle race condition here, where
 the window or process timeout could fire and before the process
 is frozen, it signals completion, which will be handled by the event loop
 after new window is started, and the completion signal from the previous
 window will arrive to the new window, and the process will be immediately
 blocked and not run at all during next window.
 */
Slice::Slice(ev::loop_ref loop,
             PowerPolicy &power_policy,
             std::function<void(Slice &, time_point)> sc_done_cb,
             Partition *sc,
             Partition *be,
             std::optional<CpuFrequencyHz> req_freq,
             cpu_set cpus)
    : sc(sc)
    , be(be)
    , cpus(std::move(cpus))
    , requested_frequency(req_freq)
    , power_policy{ power_policy }
    , sc_done_cb(std::move(sc_done_cb))
    , timer(loop)
    , completion_cb_cached{ [this](Process &) { schedule_next(std::chrono::steady_clock::now()); } }
{
    timer.set([this] { schedule_next(timeout); });
    // set lower than default priority; this makes this timeout trigger
    //  after MajorFrame window timer in case a process runs out of budget
    //  at the same moment as window end
    timer.priority = -1;
}

bool Slice::load_next_process(time_point current_time)
{
    ASSERT(running_process == nullptr);
    running_process = running_partition->seek_pending_process();
    if (running_process) {
        return true;
    }
    // no process found, we're done
    // call cb if running SC partition
    if (running_partition == sc) {
        sc_done_cb(*this, current_time);
    }
    return false;
}

/** Finds and starts next unfinished process from current_partition. */
void Slice::start_next_process(time_point current_time)
{
    if (!load_next_process(current_time)) {
        return;
    }

    auto budget = running_process->get_actual_budget();
    if (budget == budget.zero()) {
        // if 2 * budget == jitter, the budget will occasionally be zero
        //  this may simulate a process that occasionally has no work to do
        TRACE("Skipping process with empty effective budget");
        running_process->mark_completed();
        running_process = nullptr;
        return start_next_process(current_time);
    }

    using namespace std::chrono;
    schedule_logger.log(fmt::format("{:6}: cpus={} cmd='{}' budget={}\n",
                                    duration_cast<milliseconds>(current_time - start_time).count(),
                                    cpus.as_list(),
                                    running_process->argv,
                                    budget.count()));
    TRACE("Running process '{}' for '{} milliseconds'", running_process->get_pid(), budget.count());
    running_process->resume();
    timeout = current_time + budget;
    // if budget was shortened in previous window, this resets it back to full length
    running_process->reset_budget();
    timer.start(timeout);
    // FIXME: it would probably make more sense to call this inside `resume()`
    power_policy.on_process_start(*running_process);
}

void Slice::stop_current_process(bool mark_completed)
{
    ASSERT(running_process != nullptr);
    // this way, the process will run a bit longer
    //  if this call takes a long time to complete
    power_policy.on_process_end(*running_process);
    timer.stop();
    running_process->suspend();
    if (mark_completed) running_process->mark_completed();
    running_process = nullptr;
}

void Slice::start_partition(Partition *part, time_point current_time, bool move_to_first_proc)
{
    ASSERT(part != nullptr);
    running_partition = part;
    if (part) {
        part->reset(move_to_first_proc, cpus, completion_cb_cached);
    }
    start_next_process(current_time);
}

void Slice::start_sc(time_point current_time)
{
    if (!sc) {
        // if there's no SC partition, immediately signal that this slice can continue
        sc_done_cb(*this, current_time);
        return;
    }
    start_partition(sc, current_time, true);
}

void Slice::start_be(time_point current_time)
{
    if (!be) return;
    // don't move to first process, we want to continue from the last one for BE,
    //  as there aren't strict deadlines for BE processes, and it's better if each BE process gets
    //  to run occasionally than if the first one would run all the time and the rest never
    start_partition(be, current_time, false);
}

void Slice::stop(time_point current_time)
{
    timer.stop();
    // it is OK to disconnect before stopping the running process
    if (sc) sc->disconnect();
    if (be) be->disconnect();

    if (!running_process) {
        return;
    }

    // in case the remaining time is less than a millisecond, we round it to zero
    //  and consider the process completed; this lets us always schedule a whole number
    //  of milliseconds, which is nicer to work with (and consistent with how config
    //  exposes the times)
    using namespace std::chrono;
    auto remaining = duration_cast<milliseconds>(timeout - current_time);

    if (remaining == remaining.zero()) {
        // window ended approximately at the same moment when the process timed out
        // Slice::stop is called before schedule_next due to higher timer priority
        // we stop the running process and load the next one (to prepare the partition
        //  for the next window); this may call sc_done_cb if this was the last process
        //  from the SC partition
        TRACE("Process ran out of budget exactly at the window end");
        stop_current_process(true);
        load_next_process(current_time);
        // clear running_process set by load_next_process above, as we're not starting it now
        running_process = nullptr;
        return;
    }

    if (running_partition == be) {
        // we're interrupting a process from the BE partition
        // store remaining budget for next run
        running_process->set_remaining_budget(remaining);
    }

    stop_current_process(false);
}

// Called as a response to timeout or process completion.
void Slice::schedule_next(time_point current_time)
{
    stop_current_process(true);
    start_next_process(current_time);
}
