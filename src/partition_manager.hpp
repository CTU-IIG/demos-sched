#include "log.hpp"
#include "slice.hpp"

class PartitionManager
{
public:
    PartitionManager(Partitions &partitions)
        : partitions(partitions)
    {
        auto part_cb = std::bind(&PartitionManager::empty_partition_cb, this);
        for (auto &p : partitions) {
            p.set_empty_cb(part_cb);
        }
    }

    // delete move and copy constructors, as we have a pointer data member
    PartitionManager(const PartitionManager &) = delete;
    const PartitionManager &operator=(const PartitionManager &) = delete;

    /** Initializes all processes. */
    void run_process_init(std::function<void()> init_cb)
    {
        logger->debug("Process initialization started");
        this->init_cb = init_cb;
        cpu_set cpus{};
        auto part_cb = std::bind(&PartitionManager::process_init_completion_cb, this);
        for (auto &p : partitions) {
            p.reset(true, cpus, part_cb);
        }
        init_next_process();
    }

    /** Stops all processes. Processes might not terminate immediately. */
    void kill_all()
    {
        for (auto &p : partitions) {
            p.kill_all();
        }
    }

    /**
     * Sets callback that is called when all partitions
     * are empty (so there are no processes to schedule).
     */
    void set_completion_cb(std::function<void()> completion_cb)
    {
        this->completion_cb = completion_cb;
    }

private:
    Partitions &partitions;
    Partitions::iterator init_iter = partitions.begin();
    Process *initialized_proc = nullptr;
    std::function<void()> completion_cb = [] {};
    // initialized in `run_process_init`
    std::function<void()> init_cb = nullptr;

    /** Called when partition is emptied. */
    void empty_partition_cb()
    {
        for (auto &p : partitions) {
            if (!p.is_empty()) return;
        }
        completion_cb();
    }

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
        init_cb();
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
    void process_init_completion_cb()
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