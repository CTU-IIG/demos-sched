# DEmOS scheduler

Scheduler for simulation of avionics multi-core workloads on Linux.

## Compilation

Initialize git submodules:

    git update-ref refs/heads/master.trac origin/master.trac
    git submodule update --init

We use the Meson build system. On Debian-based distro, it can be
installed by:

    apt install meson

It is recommended, though not necessary, to use Meson >= 0.52. If
your distribution has an older version, you can install a new one by:

    pip3 install --user meson

Then build everything by running:

    make

Output binary will be created in `build/src/demos-sched` for the native build.

To compile a debug build of DEmOS, run `make debug` (this configures
the build type), followed by `make`.

### Cross compilation

DEmOS scheduler can be easily cross compiled thanks to [Meson cross
compilation support](https://mesonbuild.com/Cross-compilation.html).
We support cross compilation for 64bit ARM out of the box:

    sudo apt install g++-aarch64-linux-gnu
    make aarch64

The resulting binary `build-aarch64/src/demos-sched` can be copied to
the target ARM system.

## Runtime requirements

DEmOS **needs `cgroup` kernel support**, set up in
**[hybrid mode](https://github.com/systemd/systemd/blob/main/docs/CGROUP_DELEGATION.md#three-different-tree-setups-)**
to control the scheduled processes. Support for unified mode is on our roadmap. 
Currently, the following `cgroup` hierarchies are required:

 - /sys/fs/cgroup/**freezer**
 - /sys/fs/cgroup/**cpuset**
 - /sys/fs/cgroup/**unified**

## Usage

```
Usage: demos-sched <OPTIONS>
  -c <CONFIG_FILE>      path to configuration file
  -C <CONFIG>           inline configuration in YAML format
  [-p <POWER_POLICY>] name and optional arguments of the selected power management policy;
                         <POWER_POLICY> has the following form: <NAME>[:ARG1[,ARG2[,...]]]; 
                         if multiple instances of DEmOS are running in parallel, this parameter 
                         must not be passed to more than a single one
  [-g <CGROUP_NAME>]    name of root cgroups, default "demos"
                         NOTE: this name must be unique for each running instance of DEmOS
  [-m <WINDOW_MESSAGE>] print WINDOW_MESSAGE to stdout at the beginning of each window;
                         this may be useful for external synchronization with scheduler windows
  [-M <MF_MESSAGE>]     print MF_MESSAGE to stdout at the beginning of each major frame
  [-t <TIMEOUT_MS>]     scheduler timeout (milliseconds); if all scheduled processes do not exit in
                         this interval, DEmOS stops them and exits
  [-s]                  rerun itself via systemd-run to get access to unified cgroup hierarchy
  [-d]                  dump config file without execution
  [-h]                  print this message
To control logger output, use the following environment variables:
  SPDLOG_LEVEL=<level> (see https://spdlog.docsforge.com/v1.x/api/spdlog/cfg/helpers/load_levels/)
    2 loggers are defined: 'main' and 'process'; to set a different log level for the process logger,
    use e.g. 'SPDLOG_LEVEL=debug,process=info'
  DEMOS_PLAIN_LOG flag - if present, logs will not contain colors and time
  DEMOS_FORCE_COLOR_LOG flag - if present, logger will always print colored logs, 
    even when it is convinced that the attached terminal doesn't support it
```

Format of the configuration files is documented in the section [Guide for writing configurations](#Guide-for-writing-configurations).

## Project terminology

This section reviews the terminology used in later parts of this README.

### Process

Single system process (currently started as a shell command, might change in
the future), with a fixed time budget. In each window, each process from the
scheduled partition can run until it exhausts its time budget for the given
window, signals completion using the scheduler API, or the window ends.

If the process has DEmOS support, and needs some time to initialize before
starting the scheduler, it can set the optional boolean switch `init: yes`
in the configuration file - the process will then be started outside the
scheduling windows and allowed to run until it calls `demos_completed();`
to signal completion. The scheduler will start after all processes are
initialized.

**Note:** if the process does not call `demos_completed();` and `init: yes`
is set, the scheduler deadlocks.

Example config:
```yaml
- cmd: ./app1 args...
  budget: 300
  init: yes
  jitter: 10
```

### Partition
Container for a group of processes that are scheduled as a single unit. Only
a single process from each partition can run at a time - processes are ran
sequentially, starting from the first one (but see below).

There are 2 types of partitions (see slice definition below).
In safety-critical partitions, execution in each window restarts from the first
process, even if not all processes had chance to run in the last window;
in best-effort partitions, execution is continued from last unfinished process.

Example config:
```yaml
partitions:
  - name: partition1
    processes:
      - cmd: ./app1 args...
        budget: 300
      - cmd: ./app2 args...
        budget: 500
        init: yes
  - name: partition2
    processes:
      # any shell command can be used
      - cmd: yes > /dev/null
        budget: 200
...
```

### Window
"In these 3 seconds, run these slices (partitions on these cores)."

Represents a time interval when given slices are running on the CPU. Has a
defined duration (window length), then the next window is scheduled. Each
window can contain multiple slices.

Example config:
```yaml
...
windows:
  - length: 500
    slices:
      - cpu: 1
        sc_partition: SC1
        be_partition: BE1
      - cpu: 2,5-7
        be_partition: BE2
```

#### Major frame
Container for all defined windows (used internally).

### Slice
"Run this partition on these CPU cores."

Associates partitions/processes with a given CPU core set. Always nested
inside a window. 

In each slice, there are potentially 2 partitions:
- `sc_partition` = safety-critical partition
- `be_partition` = best-effort partittion

First, safety-critical partition is ran; after it finishes (either
time budget of its processes is exhausted, or the processes complete),
best-effort partition is started. SC partition is ran from the first
process in each window. BE partition is not restarted, and continues
from the last unfinished process, so that all processes get a chance
to run.

Example configs:
```yaml
...
- cpu: 1
  sc_partition: SC1
  be_partition: BE1
```
```yaml
...
- cpu: 2,5-7
  be_partition: BE2
```

## DEmOS process library
Scheduled processes can cooperate with the DEmOS scheduler by using the
`demos-sch` user library (`libdemos_sch_dep` dependency in meson in `./lib/`).

See [./lib/demos-sch.h](./lib/demos-sch.h) for documentation of the API.


## Guide for writing configurations

Configuration files are written in the YAML format. They can have
either canonical or simplified form, with the latter being
automatically converted to the former. Canonical form is most
flexible, but for same cases a bit verbose. Verbosity can be reduced
by using the simplified form. Both forms are described below.

### Canonical form of configuration file

Configuration file is a mapping with the following keys:
`set_cwd`, `demos_cpu`, `partitions` and `windows`.

- `set_cwd` (optional, default: true) is a boolean specifying whether all scheduled
  processes should have their working directory set to the directory of the configuration file; 
  this allows you to safely use relative paths inside the configuration file and scheduled programs
- `demos_cpu` (optional, default: all) is a cpulist (see `slice.cpu` below),
  defining the CPU affinity of DEmOS itself; this allows you to pin DEmOS
  to a fixed CPU to prevent interference with the scheduled processes
- `partitions` is an array of partition definitions.
  - *Partition definition* is a mapping with `name` and `processes` keys.
  - `processes` is an array of process definitions.
  - *Process definition* is mapping with `cmd`, `budget`, `jitter` and `init` keys.
    - `cmd` is a string with a command to be executed (passed to `/bin/sh -c`).
    - `budget` specifies process budget in milliseconds.
    - `jitter` (optional, default: 0) specifies jitter in milliseconds that is applied
      to the budget whenever the process is scheduled.
    - `init` (optional, default: false) is a boolean specifying if process
      should be allowed to initialize before scheduler starts.
- `windows` is an array of window definitions.
  - *Window definition* is a mapping with `length` and `slices` keys.
    - `length` defined length of the window in milliseconds.
    - `slices` is an array of slice definitions.
  - *Slice definition* is a mapping with the `cpu` key and optional
    `sc_partition` and `be_partition` keys.
    - `cpu` is a string defining scheduling CPU constraints ("cpulist"). The value can
      specify a single CPU by its zero-based number (e.g. `cpu: 1`), or a
      range of CPUs (`cpu: 0-2`), or combination of both (`cpu:
      0,2,5-7`) or string `all`, which means all available CPUs.
    - `sc_partition` and `be_partition` are strings referring to
      partition definitions by their `name`s.

Example canonical configuration can look like this:
``` yaml
set_cwd: yes

partitions:
  - name: SC1
    processes:
      - cmd: ./safety_critical_application
        budget: 300
        init: yes
  - name: BE1
    processes:
      - cmd: ./best_effort_application1
        budget: 200
  - name: BE2
    processes:
      - cmd: ./best_effort_application2
        budget: 200
      - cmd: ./best_effort_application3
        budget: 200
        
windows:
  - length: 500
    slices:
      - cpu: 1
        sc_partition: SC1
        be_partition: BE1
      - cpu: 2,5-7
        be_partition: BE2
```

### Simplified form of configuration file

- You can omit `slice` keyword. Then it is expected that there is just one slice inside window scheduled at all cpus.

  ``` yaml
  {
    partitions: [ {name: SC, processes: [{cmd: echo, budget: 100}] }],
    windows: [ {length: 500, sc_partition: SC} ]
  }
  ```

  is the same as

  ``` yaml
  partitions:
    - name: SC
      processes:
        - cmd: echo
          budget: 300
  windows:
    - length: 500
      slices:
        - cpu: 0-7
          sc_partition: SC
  ```

- You can define partition directly inside `windows` instead of partition name.

  ``` yaml
  windows: [ {length: 500, sc_partition: [{cmd: proc1, budget: 500}] } ]
  ```

  is the same as

  ``` yaml
  partitions:
    - name: anonymous_0
      processes:
        - cmd: proc1
          budget: 500
  windows:
    - length: 500
      slices:
        - cpu: 0-7
          sc_partition: anonymous_0
  ```

- If process `budget` is not set, then the default budget `0.6 * length` of
  window is set for `sc_partition` processes and `length` of window is set for
  `be_partition` processes.

- You can use `xx_processes` keyword for definition of partition by the list
  of commands:

  ``` yaml
  windows: [ {length: 500, sc_processes: [proc1, proc2]} ]
  ```


### Example configurations

``` yaml
# CPU_switching.yaml
# one process switched between two cpus
#
# all times in ms
# Major frame
windows:
  - length: 500
    slices:
      - cpu: 0
        sc_partition: SC1
  - length: 500
    slices:
      - cpu: 1
        sc_partition: SC1

partitions:
  - name: SC1
    processes:
      - cmd: yes > /dev/null
        budget: 500
```

To verify that `demos-sched` executes the partitions as intended, you
can record and visualise the execution trace with the following
commands:

    trace-cmd record -F -c -e sched_switch build/src/demos-sched -c …
    kernelshark trace.dat

The result will look like in the figure below:
![](./test_config/CPU_switching.png)

``` yaml
# more_partitions.yaml
# two slices with safety critical and best effort tasks within one window

# all times in ms
# Major frame
#period: 100 # not used, do we need this?
windows:
  - length: 1500
    slices:
      - cpu: 0
        sc_partition: SC1
        be_partition: BE1
      - cpu: 1
        sc_partition: SC2

partitions:
  - name: SC1
    processes:
      - cmd: yes > /dev/null
        budget: 500
      - cmd: yes > /dev/null
        budget: 500
  - name: BE1
    processes:
      - cmd: yes > /dev/null
        budget: 500
  - name: SC2
    processes:
      - cmd: yes > /dev/null
        budget: 1000
```

![](./test_config/more_partitions.png)

- if everything sucks run this script
    ```
    src/cleanup_crash.sh
    ```

# Citing

If you find the DEmOS tool useful for your research, please consider citing our original paper in your publication list as follows:

- O. Benedikt et al., "Thermal-Aware Scheduling for MPSoC in the Avionics Domain: Tooling and Initial Results," 2021 IEEE 27th International Conference on Embedded and Real-Time Computing Systems and Applications (RTCSA), 2021, pp. 159-168, doi: [10.1109/RTCSA52859.2021.00026](https://doi.org/10.1109/RTCSA52859.2021.00026).
