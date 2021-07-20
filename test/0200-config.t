#!/usr/bin/env bash
. testlib
plan_tests 19

# set fixed log level for config tests; otherwise if user would run something
#  like `SPDLOG_LEVEL=trace make test`, the output would include the logs and the tests would fail
export SPDLOG_LEVEL=warning

test_normalization() {
    local test_name=$1
    local cfg_in=$2
    local cfg_out_expected=$3

    local cfg_out=$(demos-sched -d -C "$cfg_in" 2>&1)
    is "$cfg_out" "$cfg_out_expected" "$test_name"
}

out=$(demos-sched -C "{ windows: [], partitions: [], garbage: garbage}" 2>&1)
is $? 1 "garbage causes failure"
like "$out" "garbage" "garbage reported"

out=$(demos-sched -C '-d' 2>&1)
is $? 1 "non-map config causes failure"
like "$out" "must be YAML mapping node" "non-map config reported"

out=$(demos-sched -d -C "{}")
like "$out" "set_cwd: false" "set_cwd defaults to false for inline config"
out=$(demos-sched -d -c <(echo "set_cwd: false"))
is $? 0 "config from FIFO file is accepted with 'set_cwd: false'"
# we cannot use <(), as demos detects special files and throws error (tested below)
tmp_config=$(mktemp)
echo "{}" > "$tmp_config"
out=$(demos-sched -d -c "$tmp_config")
like "$out" "set_cwd: true" "set_cwd defaults to true for config file"

out=$(demos-sched -C "{
    windows: [ {length: 500, sc_partition: SC} ],
    partitions: [ {name: SC, processes: [{cmd: echo, budget: 100, jitter: 250}]} ]
}" 2>&1)
is $? 1 "'jitter > 2 * budget' causes error"

test_normalization "missing slice definition" \
"{
    windows: [ {length: 500, sc_partition: SC} ],
    partitions: [ {name: SC, processes: [{cmd: echo, budget: 100}] }]
}" \
"set_cwd: false
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
      - cpu: 0-63
        sc_partition: SC"


test_normalization "partition definition in window" \
"{
    windows: [ {length: 500, sc_partition: [{cmd: proc1, budget: 500}] } ]
}" \
"set_cwd: false
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
      - cpu: 0-63
        sc_partition: anonymous_0"

test_normalization "partition definition in window with one process" \
"{
    windows: [ {length: 500, sc_partition: {cmd: proc1, budget: 500} } ]
}" \
"set_cwd: false
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
      - cpu: 0-63
        sc_partition: anonymous_0"

test_normalization "default budget" \
"{
    windows: [ {length: 500, sc_partition: [{cmd: proc1}] } ]
}" \
"set_cwd: false
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
      - cpu: 0-63
        sc_partition: anonymous_0"

# This is the case, why we need sc_processes and cannot reuse
# sc_partition here.
test_normalization "process as string" \
"{
    windows: [ {length: 500, sc_processes: proc} ]
}" \
"set_cwd: false
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
      - cpu: 0-63
        sc_partition: anonymous_0"

test_normalization "Processes as string" \
"{
    windows: [ {length: 500, sc_processes: [proc1, proc2]} ]
}" \
"set_cwd: false
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
      - cpu: 0-63
        sc_partition: anonymous_0"

test_normalization "short-form jitter" \
"{
    windows: [ {length: 100, sc_partition: SC} ],
    partitions: [ {name: SC, processes: [{cmd: echo, budget: 100, jitter: 50}] }]
}" \
"set_cwd: false
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
      - cpu: 0-63
        sc_partition: SC"

export DEMOS_PLAIN_LOG=1
test_normalization "missing budget in canonical config" \
        "{partitions: [ name: p1, processes: [ cmd: proc1 ] ], windows: [{length: 100, sc_partition: p1}]}" \
		">>> [error] Exception: Missing budget in process definition"
test_normalization "missing cpu set for slice" \
        "windows: [{length: 100, slices: [{}]}]" \
        ">>> [error] Exception: Missing cpu set in slice definition (\`cpu\` property)"
test_normalization "set_cwd: yes for inline config string fails" \
		"set_cwd: yes" \
		">>> [error] Exception: 'set_cwd' cannot be used in inline config string"
out=$(demos-sched -d -c <(echo "{}"))
is $? 1 "config from FIFO file is not accepted without 'set_cwd: false'"