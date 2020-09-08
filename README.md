# DEmOS scheduler

Scheduler for simulation of avionics multi-core workloads on Linux.

## Compilation

Initialize git submodules:

    git submodule update --init
    git submodule foreach --recursive 'if [ $(git config remote.origin.url) = . ]; then git config remote.origin.url "$toplevel"; fi'

We use the Meson build system. On Debian-based distro, it can be
installed by:

    apt install meson

It is recommended, though not necessary, to uses Meson >= 0.52. If
your distribution has an older version, you can install a new one by:

    pip3 install --user meson

Then build everything by running:

    make

### Cross compilation

DEmOS scheduler can be easily cross compiled thanks to [Meson cross
compilation support][cross]. We support cross compilation for 64bit
ARM out of the box:

    sudo apt install g++-aarch64-linux-gnu
	make aarch64

The resulting binary `build-aarch64/src/demos-sched` can be copied to
the target ARM system.

[cross]: https://mesonbuild.com/Cross-compilation.html

## Usage

    Usage: demos-sched -c <CONFIG_FILE> [-h] [-g <CGROUP_NAME>]
      -c <CONFIG_FILE>   path to configuration file
      -C <CONFIG>        in-line configuration in YAML format
      -g <CGROUP_NAME>   name of root cgroups, default "demos"
      -d                 dump config file without execution
      -h                 print this message


Format of configuration files is documented in the next section.

## Guide for writing configurations

Configuration files are written in the YAML format. They can have
either canonical or simplified form, with the latter being
automatically converted to the former. Canonical form is most
flexible, but for same cases a bit verbose. Verbosity can be reduced
by using the simplified form. Both forms are described bellow.

### Canonical form of configuration file

- Configuration file is a mapping with the following keys:
  `partitions` and `windows`.
- `partitions` is an array of partition definitions.
- *Partition definition* is a mapping with `name` and `processes` keys.
- `processes` is an array of process definitions.
- *Process definition* is mapping with `cmd` and `budget` keys.
- `cmd` is a string with a command to be executed (passed to `/bin/sh -c`).
- `budget` specifies process budget in milliseconds.
- `windows` is an array of window definitions.
- *Window definition* is a mapping with `length` and `slices` keys.
- `length` defined length of the window in milliseconds.
- `slices` is an array of slice definitions.
- *Slice definition* is a mapping with the `cpu` key and optional
  `sc_partition` and `be_partition` keys.
- `cpu` is a string defining scheduling CPU constraints. The value can
  specify a single CPU by its zero-based number (e.g. `cpu: 1`), or a
  range of CPUs (`cpu: 0-2`), or combination of both (`cpu: 0,2,5-7`).
- `sc_partition` and `be_partition` are strings referring to
  partition definitions by their `name`s.

Example canonical configuration can look like this:
``` yaml
partitions:
  - name: SC1
    processes:
      - cmd: ./safety_critical_application
        budget: 300
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
        be_partitions: BE1
      - cpu: 2,5-7
        be_partitions: BE2
```

### Simplified form of configuration file

- You can omit `slice` keyword. Then it is expected that there is just one slice inside window scheduled at all cpus.

``` yaml
windows: [ {length: 500, sc_partition: SC} ],
partitions: [ {name: SC, processes: [{cmd: echo, budget:100}] }]
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

- If process `budget` is not set, then the default budget `0.6 * length` of window is set for `sc_partition` processes and `length` of window is set for `be_partition` processes.
- You can use `xx_processes` keyword for definition of partition by the list of commands.

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
    src/release_agent.sh
    ```

