#include "log.hpp"
#include "slice.hpp"

class ProcessInitializer
{
public:
    ProcessInitializer(Partitions &partitions)
        : partitions(partitions)
    {}

    // delete move and copy constructors, as we have a pointer data member
    ProcessInitializer(const ProcessInitializer &) = delete;
    const ProcessInitializer &operator=(const ProcessInitializer &) = delete;

    /** Initializes all processes. */
    void run_process_init(std::function<void()> cb)
    {
        logger->debug("Process initialization started");
        this->cb = cb;
        cpu_set cpus{};
        auto part_cb = std::bind(&ProcessInitializer::init_completion_cb, this);
        for (auto &p : partitions) {
            p.reset(true, cpus, part_cb);
        }
        init_next_process();
    }

private:
    Partitions &partitions;
    Partitions::iterator init_iter = partitions.begin();
    Process *initialized_proc = nullptr;
    std::function<void()> cb = nullptr;

    Process *find_next_uninitialized_process()
    {
        Process *proc;
        // find next uninitialized process
        for (; init_iter != partitions.end(); init_iter++) {
            proc = init_iter->find_unfinished_process();
            if (proc) return proc;
        }
        return nullptr;
    }

    void finish_process_init()
    {
        for (auto &p : partitions) {
            p.disconnect();
        }
        logger->debug("Process initialization finished");
        cb();
    }

    /** Initiates single process. Completion is handled by init_next_process(). */
    void init_process(Process *p)
    {
        if (!p->needs_initialization()) {
            p->mark_completed();
            init_next_process();
            return;
        }

        logger->trace("Initializing process '{}'", p->get_pid());
        // resume the process; it should stop on its own after initialization is completed
        // then, it will call init_next_process as callback
        p->unfreeze();
    }

    /** Called as a completion callback from process. */
    void init_completion_cb()
    {
        initialized_proc->freeze();
        initialized_proc->mark_completed();
        init_next_process();
    }

    void init_next_process()
    {
        initialized_proc = find_next_uninitialized_process();
        if (initialized_proc) {
            init_process(initialized_proc);
        } else {
            finish_process_init();
        }
    }
};