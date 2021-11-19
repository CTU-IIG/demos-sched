#include "process.hpp"
#include "lib/check_lib.hpp"
#include "log.hpp"
#include "partition.hpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <lib/assert.hpp>
#include <sys/eventfd.h>

using namespace std::placeholders;
using milliseconds = std::chrono::milliseconds;

static std::mt19937 gen([]() -> unsigned {
    const char *seed = getenv("DEMOS_RAND_SEED");
    if (seed) {
        return std::stoi(std::string(seed));
    } else {
        // C++ <random> is broken beyond any reason
        // this initialisation is not really uniform, but we don't need that here
        // see https://stackoverflow.com/questions/45069219/how-to-succinctly-portably-and-thoroughly-seed-the-mt19937-prng
        return std::random_device{}();
    }
}());

Process::Process(ev::loop_ref loop,
                 const std::string &name,
                 Partition &partition,
                 std::string argv,
                 std::optional<std::filesystem::path> working_dir,
                 milliseconds budget,
                 milliseconds budget_jitter,
                 std::optional<CpuFrequencyHz> req_freq,
                 bool has_initialization)
    : part(partition)
    , requested_frequency(req_freq)
    , argv(std::move(argv))
    // budget +- (jitter / 2)
    , jitter_distribution_ms(-budget_jitter.count() / 2,
                             budget_jitter.count() - budget_jitter.count() / 2)
    , loop(loop)
    , efd_continue(CHECK(eventfd(0, EFD_SEMAPHORE)))
    , cge(loop,
          partition.cge,
          name,
          std::bind(&Process::populated_cb, this, _1)) // NOLINT(modernize-avoid-bind)
    , cgf(partition.cgf, name)
    , working_dir(std::move(working_dir))
    , budget(budget)
    , actual_budget(budget)
    , has_initialization(has_initialization)
{
    // check that randomized budget cannot be negative
    // actual budget is `budget +- (jitter / 2)`
    ASSERT(2 * budget >= budget_jitter);
    reset_budget(); // Apply initial budget jitter (if needed)
    suspend();
    completed_w.set(std::bind(&Process::completed_cb, this)); // NOLINT(modernize-avoid-bind)
    completed_w.start();
    child_w.set<Process, &Process::child_terminated_cb>(this);
}

void Process::exec()
{
    // create new process
    pid = CHECK(fork());
    system_process_spawned = true;
    killed = false;

    // launch new process
    if (pid == 0) {
        // CHILD PROCESS
        char val[100];
        // pass configuration to the process library
        // passed descriptors are used for communication with the library:
        //  - completed_fd is used to read completion messages from process
        //  - efd_continue is used to signal continuation to process
        sprintf(val, "%d,%d,%d", completed_w.get_fd(), efd_continue, has_initialization);
        CHECK(setenv("DEMOS_PARAMETERS", val, 1));
        if (working_dir) {
            CHECK(chdir(working_dir->c_str()));
        }
        CHECK(execl("/bin/sh", "/bin/sh", "-c", argv.c_str(), nullptr));
        // END CHILD PROCESS
    } else {
        // PARENT PROCESS
        logger_process->debug(
          "Running '{}' as PID '{}' (partition '{}')", argv, pid, part.get_name());
        child_w.start(pid, 0);
        // TODO: shouldn't we do this in the child process, so that we know the process
        //  is frozen before we start the command? as it is now, I think there's a possible
        //  race condition where the started command could run before we move it into the freezer
        // add process to cgroup (echo PID > cgroup.procs)
        cge.add_process(pid);
        cgf.add_process(pid);
        // END PARENT PROCESS
    }
}

void Process::kill()
{
    if (!is_spawned()) return;
    cgf.freeze();
    cgf.kill_all();
    cgf.unfreeze();
    killed = true;
}

void Process::suspend()
{
    cgf.freeze();
    if (is_spawned()) {
        TRACE_PROCESS("Suspended process '{}' (partition '{}')", pid, part.get_name());
    }
}

void Process::resume()
{
    ASSERT(is_spawned());
    TRACE_PROCESS("Resuming process '{}' (partition '{}')", pid, part.get_name());
    uint64_t buf = 1;
    if (demos_completed) {
        CHECK(write(efd_continue, &buf, sizeof(buf)));
        demos_completed = false;
    }
    cgf.unfreeze();
}

milliseconds Process::get_actual_budget() const
{
    return actual_budget;
}

bool Process::needs_initialization() const
{
    return has_initialization;
}

bool Process::is_pending() const
{
    return is_spawned() && !completed;
}

void Process::mark_completed()
{
    completed = true;
    reset_budget();
}

void Process::mark_uncompleted()
{
    completed = false;
}

bool Process::is_spawned() const
{
    return system_process_spawned;
}

pid_t Process::get_pid() const
{
    return pid;
}

void Process::set_remaining_budget(milliseconds next_budget)
{
    ASSERT(next_budget > next_budget.zero());
    ASSERT(next_budget < budget);
    actual_budget = next_budget;
    TRACE_PROCESS(
      "Next budget for process '{}' shortened to '{} milliseconds'.", pid, next_budget.count());
}

void Process::reset_budget()
{
    actual_budget = budget + milliseconds(jitter_distribution_ms(gen));
    ASSERT(actual_budget >= milliseconds::zero());
}

/** Called when the cgroup is empty and we want to signal it to the parent partition. */
void Process::handle_end()
{
    system_process_spawned = false;
    part.proc_exit_cb(*this);
}

/**
 * Called when the process signalizes completion in the current window
 * by calling demos_completed().
 *
 * This either means that initialization finished (if 'init' param in config was set to true),
 * or that the work for current window is done.
 *
 * TODO: shouldn't we rename the whole concept to yield,
 *  to be more consistent with POSIX terminology?
 */
void Process::completed_cb()
{
    TRACE_PROCESS("Process '{}' completed (cmd: '{}')", pid, argv);

    demos_completed = true;
    // TODO: wouldn't it be better to call `suspend()` here?
    part.completed_cb(*this);
}

/**
 * Called when the population of cgroup changes
 * (either it is now empty, or new process was added).
 *
 * @param populated - true if the cgroup currently contains any processes
 */
void Process::populated_cb(bool populated)
{
    if (populated) return;

    if (waiting_for_empty_cgroup) {
        waiting_for_empty_cgroup = false;
        logger_process->trace("Cgroup for process '{}' is empty", pid);
        handle_end();
    }
}

/** Called when our spawned child process terminates. */
void Process::child_terminated_cb(ev::child &w, [[maybe_unused]] int revents)
{
    w.stop();
    int wstatus = w.rstatus;
    // clang-format off
    if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) != 0) {
        logger_process->warn("Process '{}' exited with status {} (partition: '{}', cmd: '{}')",
                             pid, WEXITSTATUS(wstatus), part.get_name(), argv);
    } else if (WIFSIGNALED(wstatus) && !killed) { // don't warn if killed by scheduler
        logger_process->warn("Process '{}' terminated by signal {} (partition: '{}', cmd: '{}')",
                             pid, WTERMSIG(wstatus), part.get_name(), argv);
    } else {
        logger_process->debug("Process '{}' exited (partition '{}', cmd: '{}')",
                              pid, part.get_name(), argv);
    }
    // clang-format on

    if (cge.read_populated_status()) {
        // direct child exited, but the cgroup is not empty
        // most probably, the child process left behind an orphaned child
        logger_process->trace("Original process '{}' ended, but it has orphaned children", pid);
        // we'll let `populated_cb` call `handle_end()` when the cgroup is emptied
        waiting_for_empty_cgroup = true;
    } else {
        handle_end(); // cgroup is empty
    }
}
