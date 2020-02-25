#ifndef CGROUP_HPP
#define CGROUP_HPP

// mkdir, open
#include <sys/stat.h>
#include <sys/types.h>
// open
#include <fcntl.h>
// write
#include <unistd.h>

#include <bitset>
#include <err.h>
#include <errno.h>
#include <ev++.h>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <signal.h>
#include <sys/types.h>
#include <vector>

// maximum supported number of processors
#define MAX_NPROC 8
// cpu usage mask
typedef std::bitset<MAX_NPROC> Cpu;

class Process;

class Cgroup
{
public:
    Cgroup() {}
    Cgroup(std::string path, bool may_exist = false);
    Cgroup(std::string parent_path, std::string name);
    Cgroup(Cgroup &parent, std::string name);
    ~Cgroup();

    void add_process(pid_t pid);
    // void add(const Process& proc); // ????
    void kill_all();

    // delete copy constructor
    Cgroup(const Cgroup &) = delete;
    Cgroup &operator=(const Cgroup &) = delete;

    // Move constructor/assignment
    Cgroup(Cgroup &&other) = default;
    Cgroup &operator=(Cgroup &&) = default;

    std::string get_path() const;

protected:
    bool remove = true;
    std::string path;
};

class CgroupFreezer : public Cgroup
{
public:
    CgroupFreezer(std::string parent_path, std::string name);
    CgroupFreezer(Cgroup &parent, std::string name);

    void freeze();
    void unfreeze();

private:
    int fd_state;
};

class CgroupCpuset : public Cgroup
{
public:
    CgroupCpuset(std::string parent_path, std::string name);
    CgroupCpuset(Cgroup &parent, std::string name);
    void set_cpus(std::string cpus);

private:
    int fd_cpus;
};

class CgroupEvents : public Cgroup
{
public:
    CgroupEvents(ev::loop_ref loop,
                 std::string parent_path,
                 std::string name,
                 std::function<void(bool)> populated_cb);
    CgroupEvents(ev::loop_ref loop,
                 CgroupEvents &parent,
                 std::string name,
                 std::function<void(bool)> populated_cb);
    CgroupEvents(ev::loop_ref loop,
                 Cgroup &parent,
                 std::string name,
                 std::function<void(bool)> populated_cb);
    ~CgroupEvents();

private:
    int fd_events;
    ev::io events_w;
    std::function<void(bool)> populated_cb;
    void clean_cb(ev::io &w, int revents);
};

#endif // CGROUP_HPP
