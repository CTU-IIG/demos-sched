#pragma once

#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <random>
#include <unistd.h>
#include <vector>

#include "cgroup.hpp"
#include "cpufreq_policy.hpp"
#include "evfd.hpp"
#include "timerfd.hpp"

class Partition;

/**
 * Represents a single system process (started as a shell command), with a fixed time budget.
 * Process budget is per window cycle - in each window cycle, given process can
 * only run that long before it is suspended and the next process from the parent
 * partition is ran (if such process exists).
 *
 * Starts in frozen state.
 */
class Process
{
public:
    Process(ev::loop_ref loop,
            const std::string &name,
            Partition &partition,
            std::string argv,
            std::optional<std::filesystem::path> working_dir,
            std::chrono::milliseconds budget,
            std::chrono::milliseconds budget_jitter,
            std::optional<CpuFrequencyHz> req_freq,
            bool has_initialization = false);

    /** Spawns the underlying system process. */
    void exec();

    /**
     * Kills the process, and all children in its cgroup.
     *
     * Does not wait for exit, you should wait for the exit of all processes
     * on the parent partition using `set_empty_cb`.
     */
    void kill();

    /**
     * Suspends this process and all children.
     * Internally, this freezes the underlying cgroup.
     */
    void suspend();
    /**
     * Resumes this process and all children.
     * Internally, this unfreezes the underlying cgroup.
     */
    void resume();

    /**
     * Sets budget for the next window.
     *
     * Used for BE processes that are suspended due to a window timeout before
     * they use up the whole budget in a single window.
     */
    void set_remaining_budget(std::chrono::milliseconds next_budget);

    [[nodiscard]] std::chrono::milliseconds get_actual_budget() const;
    [[nodiscard]] pid_t get_pid() const;
    [[nodiscard]] bool needs_initialization() const;
    [[nodiscard]] bool is_spawned() const;
    [[nodiscard]] bool is_pending() const;

    void mark_completed();
    void mark_uncompleted();

    Partition &part;
    const std::optional<CpuFrequencyHz> requested_frequency;
    const std::string argv;

private:
    std::uniform_int_distribution<long> jitter_distribution_ms;

    const ev::loop_ref loop;
    ev::evfd completed_w{ loop };
    ev::child child_w{ loop };
    int efd_continue; // new period eventfd

    CgroupEvents cge;
    CgroupFreezer cgf;

    const std::optional<std::filesystem::path> working_dir;
    const std::chrono::milliseconds budget;
    std::chrono::milliseconds actual_budget;
    const bool has_initialization;
    bool completed = false;
    bool demos_completed = false;
    // cannot be replaced by `pid >= 0`, as we want
    //  to keep pid even after process exits, to be
    //  able to correctly handle some delayed events
    /** Indicates if the process or any of its (possibly detached) children are running. */
    bool system_process_spawned = false;
    /**
     * Indicates if the original spawned process exited and
     * we are waiting until all child processes also exit.
     */
    bool waiting_for_empty_cgroup = false;
    /**
     * Indicates if the process was killed using `Process::kill()`.
     * Used to print a warning when process ends unexpectedly.
     */
    bool killed = false;
    pid_t pid = -1;

    void handle_end();
    void populated_cb(bool populated);
    void completed_cb();
    void child_terminated_cb(ev::child &w, [[maybe_unused]] int revents);
    void reset_budget();
};
