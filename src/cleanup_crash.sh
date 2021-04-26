#!/usr/bin/env bash
set -Eeuo pipefail
shopt -s extglob nullglob globstar

# NOTE: hybrid cgroup mode is assumed here;
#  after switch to unified cgroups, this will need to be updated

if [[ $EUID > 0 ]]; then
    >&2 echo "Please run this script as root"
    exit 1
fi


# stop all running demos instances
# found using the cgroup name, which includes scheduler PID
freezer=(/sys/fs/cgroup/freezer/demos-+([0-9]))
for pid in "${freezer[@]##*-}"; do
    if [[ ! -d "/proc/$pid" ]]; then continue; fi
    echo "Stopping demos instance: $pid"
    kill -INT "$pid"
done
# give time to demos instances to exit; ideally, this should be a `wait` with timeout
#  (in case the scheduler process is unresponsive)
sleep 0.1


# find all demos cgroups (demos-<scheduler_pid>)
freezer=(/sys/fs/cgroup/freezer/demos-+([0-9]))
cpuset=(/sys/fs/cgroup/cpuset/demos-+([0-9]))
unified=(
    /sys/fs/cgroup/unified/demos-+([0-9])
    /sys/fs/cgroup/unified/user.slice/user-+([0-9]).slice/*/demos-+([0-9])
    /sys/fs/cgroup/unified/user.slice/user-+([0-9]).slice/*/run-*/demos-+([0-9])
)
echo "Found freezer:" "${freezer[@]}"
echo "Found cpuset:" "${cpuset[@]}"
echo "Found unified:" "${unified[@]}"

if [[ ${#freezer[@]} -gt 0 ]]; then
    # https://askubuntu.com/questions/343727/filenames-with-spaces-breaking-for-loop-find-command
    # kill all processes in demos freezers
    find "${freezer[@]}" -name 'cgroup.procs' -print0 | while IFS= read -r -d '' proc_file; do
        while IFS= read -r line; do
            echo "Killing process: $line"
            kill $line
        done <"$proc_file"
    done

    # unfreeze all demos freezers - this allows the processes to end
    find "${freezer[@]}" -name 'freezer.state' -print0 | while IFS= read -r -d '' state_file; do
        echo "THAWED" >"$state_file"
    done
fi

# remove all demos cgroups
for cgdir in "${freezer[@]}" "${cpuset[@]}" "${unified[@]}"; do
    # first, delete grandchildren (proc cgroups), then delete children (partition cgroups), then whole cgroup
    echo "Removing cgroup: $cgdir"
    # `|| true`, because under systemd-run, cgroup root seems to be automatically deleted
    #  with last child and `find` then cannot delete it and complains
    find "$cgdir" -maxdepth 2 -type d -delete || true
done


# reset CPU frequency scaling state
if [[ -f "/sys/devices/system/cpu/intel_pstate/status" ]]; then
    # for Intel, it is enough to reset driver to active mode
    echo "Resetting intel_pstate driver to 'active' state"
    echo "active" >"/sys/devices/system/cpu/intel_pstate/status"
elif [[ -d "/sys/devices/system/cpu/cpufreq" ]]; then
    echo "Resetting cpufreq governors to 'schedutil'"
    # for others, switch all policies to schedutil (and hope that was the default)
    echo "schedutil" | tee /sys/devices/system/cpu/cpufreq/policy*/scaling_governor >/dev/null
fi
