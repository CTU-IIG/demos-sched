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
#include <cerrno>
#include <csignal>
#include <err.h>
#include <ev++.h>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <sys/types.h>
#include <vector>

#include "cpu_set.hpp"

class Cgroup
{
public:
    Cgroup() : path{} {}

    explicit Cgroup(const std::string &path, bool may_exist = false);
    Cgroup(const std::string &parent_path, const std::string &name);
    Cgroup(const Cgroup &parent, const std::string &name);
    ~Cgroup();

    void add_process(pid_t pid);
    void kill_all();

    // delete copy constructor
    Cgroup(const Cgroup &) = delete;
    Cgroup &operator=(const Cgroup &) = delete;

    // move constructor/assignment
    Cgroup(Cgroup &&other) = default;
    Cgroup &operator=(Cgroup &&) = default;

    [[nodiscard]] std::string get_path() const;

protected:
    bool remove = true;
    std::string path;
};

class CgroupFreezer : public Cgroup
{
public:
    CgroupFreezer(const std::string &parent_path, const std::string &name);
    CgroupFreezer(const Cgroup &parent, const std::string &name);
    ~CgroupFreezer();

    void freeze();
    void unfreeze();

private:
    const int fd_state;
};

class CgroupCpuset : public Cgroup
{
public:
    CgroupCpuset(const std::string &parent_path, const std::string &name);
    CgroupCpuset(const Cgroup &parent, const std::string &name);
    ~CgroupCpuset();

    void set_cpus(cpu_set cpus);

private:
    int fd_cpus;
    cpu_set current_cpus = { 0 };
};

class CgroupUnified : public Cgroup
{
public:
    CgroupUnified(const std::string &parent_path, const std::string &name);
    CgroupUnified(const Cgroup &parent, const std::string &name);
    ~CgroupUnified();

    [[nodiscard]] bool read_populated_status() const;

protected:
    int fd_events;
};

/**
 * Monitors cgroup.events (Cgroups v2) for changes and on every change
 * calls populated_cb callback with information whether the cgroup is
 * populated or not.
 */
class CgroupEvents : public CgroupUnified
{
public:
    CgroupEvents(ev::loop_ref loop,
                 const std::string &parent_path,
                 const std::string &name,
                 const std::function<void(bool)> &populated_cb);

    CgroupEvents(ev::loop_ref loop,
                 const Cgroup &parent,
                 const std::string &name,
                 const std::function<void(bool)> &populated_cb);

    ~CgroupEvents();

private:
    ev::io events_w;
    std::function<void(bool)> populated_cb;
    void event_cb();
};

#endif // CGROUP_HPP
