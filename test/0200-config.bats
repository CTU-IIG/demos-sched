#!/usr/bin/env bats
load testlib

# set fixed log level for config tests; otherwise if user would run something
#  like `SPDLOG_LEVEL=trace make test`, the output would include the logs and the tests would fail
export SPDLOG_LEVEL=warning

test_normalization() {
    local cfg_in=$1
    local cfg_out_expected=$2

    run demos-sched -d -C "$cfg_in"
    # log the output for easier debugging when a test fails
    echo "$output"
    [[ $output = "$cfg_out_expected" ]]
}

@test "garbage in config is reported" {
    run ! demos-sched -C "{ windows: [], partitions: [], garbage: garbage}"
    [[ $output =~ "garbage" ]]
}

@test "non-map config reported" {
    run -1 demos-sched -C '-d'
    [[ $output =~ "must be YAML mapping node" ]]
}

@test "set_cwd defaults to false for inline config" {
    run -0 demos-sched -d -C "{}"
    [[ $output =~ "set_cwd: false" ]]
}

@test "config from FIFO file is accepted with 'set_cwd: false'" {
    run -0 demos-sched -d -c <(echo "set_cwd: false")
}

@test "set_cwd defaults to true for config file" {
    # we cannot use <(), as demos detects special files and throws error (tested below)
    tmp_config=$(mktemp)
    echo "{}" > "$tmp_config"
    run -0 demos-sched -d -c "$tmp_config"
    [[ $output =~ "set_cwd: true" ]]
}

@test "'jitter > 2 * budget' causes an error" {
    run -1 demos-sched -C \
"{
    windows: [ {length: 500, sc_partition: SC} ],
    partitions: [ {name: SC, processes: [{cmd: echo, budget: 100, jitter: 250}]} ]
}"
}

@test "missing slice definition" {
    test_normalization \
"{
    windows: [ {length: 500, sc_partition: SC} ],
    partitions: [ {name: SC, processes: [{cmd: echo, budget: 100}] }]
}" \
"set_cwd: false
demos_cpu: all
partitions:
  - name: SC
    processes:
      - cmd: echo
        budget: 100
        jitter: 0
        init: false
windows:
  - length: 500
    slices:
      - cpu: all
        sc_partition: SC"
}

@test "partition definition in window" {
    test_normalization \
"{
    windows: [ {length: 500, sc_partition: [{cmd: proc1, budget: 500}] } ]
}" \
"set_cwd: false
demos_cpu: all
partitions:
  - name: anonymous_0
    processes:
      - cmd: proc1
        budget: 500
        jitter: 0
        init: false
windows:
  - length: 500
    slices:
      - cpu: all
        sc_partition: anonymous_0"
}

@test "empty window with only 'length' is kept empty" {
    test_normalization \
"{windows: [{length: 1000}]}" \
"set_cwd: false
demos_cpu: all
partitions: ~
windows:
  - length: 1000"
}

@test "partition definition in window with one process" {
    test_normalization \
"{
    windows: [ {length: 500, sc_partition: {cmd: proc1, budget: 500} } ]
}" \
"set_cwd: false
demos_cpu: all
partitions:
  - name: anonymous_0
    processes:
      - cmd: proc1
        budget: 500
        jitter: 0
        init: false
windows:
  - length: 500
    slices:
      - cpu: all
        sc_partition: anonymous_0"
}

@test "default budget" {
    test_normalization \
"{
    windows: [ {length: 500, sc_partition: [{cmd: proc1}] } ]
}" \
"set_cwd: false
demos_cpu: all
partitions:
  - name: anonymous_0
    processes:
      - cmd: proc1
        budget: 300
        jitter: 0
        init: false
windows:
  - length: 500
    slices:
      - cpu: all
        sc_partition: anonymous_0"
}

# This is the case, why we need sc_processes and cannot reuse
# sc_partition here.
@test "process as string" {
    test_normalization \
"{
    windows: [ {length: 500, sc_processes: proc} ]
}" \
"set_cwd: false
demos_cpu: all
partitions:
  - name: anonymous_0
    processes:
      - cmd: proc
        budget: 300
        jitter: 0
        init: false
windows:
  - length: 500
    slices:
      - cpu: all
        sc_partition: anonymous_0"
}

@test "Processes as string" {
    test_normalization \
"{
    windows: [ {length: 500, sc_processes: [proc1, proc2]} ]
}" \
"set_cwd: false
demos_cpu: all
partitions:
  - name: anonymous_0
    processes:
      - cmd: proc1
        budget: 150
        jitter: 0
        init: false
      - cmd: proc2
        budget: 150
        jitter: 0
        init: false
windows:
  - length: 500
    slices:
      - cpu: all
        sc_partition: anonymous_0"
}

@test "short-form jitter" {
    test_normalization \
"{
    windows: [ {length: 100, sc_partition: SC} ],
    partitions: [ {name: SC, processes: [{cmd: echo, budget: 100, jitter: 50}] }]
}" \
"set_cwd: false
demos_cpu: all
partitions:
  - name: SC
    processes:
      - cmd: echo
        budget: 100
        jitter: 50
        init: false
windows:
  - length: 100
    slices:
      - cpu: all
        sc_partition: SC"
}

@test "missing budget in canonical config" {
    export DEMOS_PLAIN_LOG=1
    test_normalization \
        "{partitions: [ name: p1, processes: [ cmd: proc1 ] ], windows: [{length: 100, sc_partition: p1}]}" \
        ">>> [error] Exception: Missing budget in process definition"
}

@test "missing cpu set for slice" {
    export DEMOS_PLAIN_LOG=1
    test_normalization \
        "windows: [{length: 100, slices: [{}]}]" \
        ">>> [error] Exception: Missing cpu set in slice definition (\`cpu\` property)"
}

@test "set_cwd: yes for inline config string fails" {
    export DEMOS_PLAIN_LOG=1
    test_normalization \
        "set_cwd: yes" \
        ">>> [error] Exception: 'set_cwd' cannot be used in inline config string"
}

@test "config from FIFO file is not accepted without 'set_cwd: false'" {
    run -1 demos-sched -d -c <(echo "{}")
}
