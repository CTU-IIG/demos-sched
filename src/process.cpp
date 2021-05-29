#include "process.hpp"
#include "lib/check_lib.hpp"
#include "log.hpp"
#include "partition.hpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <lib/assert.hpp>
#include <spawn.h>
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
    suspend();
    completed_w.set(std::bind(&Process::completed_cb, this)); // NOLINT(modernize-avoid-bind)
    completed_w.start();
    child_w.set<Process, &Process::child_terminated_cb>(this);
}

/** Creates a copy of environ, with `offset` null pointers at the beginning for custom use. */
static const char **copy_environ(size_t offset)
{
    // count environ size
    char **env = environ;
    size_t size = 1; // 1 to include the terminating nullptr
    for (; *env != nullptr; env++)
        size++;

    // create new environment block with offset
    const char **env_copy = new const char *[size + offset];
    std::memcpy(env_copy + offset, environ, sizeof(char *) * size);
    return env_copy;
}

void Process::exec()
{
    // create new environment block that can be modified and passed to posix_spawn for each process
    static const char **copied_envp = copy_environ(1);
    // kept to reset after each child process spawn
    static const char *original_cwd = getcwd(nullptr, 0);

    // pass configuration to the process library
    // passed descriptors are used for communication with the library:
    //  - completed_fd is used to read completion messages from process
    //  - efd_continue is used to signal continuation to process
    char parameter_str[100];
    sprintf(parameter_str,
            "DEMOS_PARAMETERS=%d,%d,%d",
            completed_w.get_fd(),
            efd_continue,
            has_initialization);
    copied_envp[0] = parameter_str;

    if (working_dir) {
        // posix_spawn currently doesn't have any way to set working directory
        //  for the child process; we hack around it by setting working directory
        //  for the main DEmOS process, calling posix_spawn, and then changing
        //  it back to the original value
        CHECK(chdir(working_dir->c_str()));
    }

    // use /bin/sh to start the process; this lets us avoid custom argument parsing
    // in the future, it would be nicer to start the process directly
    const char *const spawn_argv[] = { "/bin/sh", "-c", argv.c_str(), nullptr };
    // posix_spawn argv and envp arguments are not const, but according to POSIX,
    //  they should not be modified by posix_spawn, so we just const_cast them
    CHECK(posix_spawn(&pid,
                      "/bin/sh",
                      nullptr,
                      nullptr,
                      const_cast<char *const *>(spawn_argv),
                      const_cast<char *const *>(copied_envp)));

    if (working_dir) {
        // reset the working directory back to original
        CHECK(chdir(original_cwd));
    }

    system_process_spawned = true;
    killed = false;

    logger_process->debug("Running '{}' as PID '{}' (partition '{}')", argv, pid, part.get_name());
    child_w.start(pid, 0);
    // TODO: shouldn't we do this in the child process, so that we know the process
    //  is frozen before we start the command? as it is now, I think there's a possible
    //  race condition where the started command could run before we move it into the freezer
    // add process to cgroup (echo PID > cgroup.procs)
    cge.add_process(pid);
    cgf.add_process(pid);
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

milliseconds Process::get_actual_budget()
{
    if (budget != actual_budget) {
        // a modified budget was stored, which should already be randomized, keep it
        ASSERT(actual_budget > actual_budget.zero());
        return actual_budget;
    }
    auto b = actual_budget + milliseconds(jitter_distribution_ms(gen));
    ASSERT(b >= b.zero());
    return b;
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
    actual_budget = budget;
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
