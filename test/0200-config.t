#!/usr/bin/env bash
. testlib
plan_tests 9

test_normalization() {
    local test_name=$1
    local cfg_in=$2
    local cfg_out_expected=$3

    local cfg_out=$(demos-sched -d -C "$cfg_in" 2>&1)
    is "$cfg_out" "$cfg_out_expected" "$test_name"
}

out=$(demos-sched -C "{ windows: [], partitions: [], garbage: garbage}" 2>&1)
is $? 1 "garbage causes failure"
like "$out" garbage "garbage reported"

test_normalization "missing slice definition" \
"{
    windows: [ {length: 500, sc_partition: SC} ],
    partitions: [ {name: SC, processes: [{cmd: echo, budget: 100}] }]
}" \
"partitions:
  - name: SC
    processes:
      - cmd: echo
        budget: 100
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
"partitions:
  - name: anonymous_0
    processes:
      - cmd: proc1
        budget: 500
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
"partitions:
  - name: anonymous_0
    processes:
      - cmd: proc1
        budget: 500
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
"partitions:
  - name: anonymous_0
    processes:
      - cmd: proc1
        budget: 300
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
"partitions:
  - name: anonymous_0
    processes:
      - cmd: proc
        budget: 300
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
"partitions:
  - name: anonymous_0
    processes:
      - cmd: proc1
        budget: 150
        init: false
      - cmd: proc2
        budget: 150
        init: false
windows:
  - length: 500
    slices:
      - cpu: 0-63
        sc_partition: anonymous_0"

export DEMOS_PLAIN_LOG=1
test_normalization "missing budget in canonical config" \
		   "{partitions: [ name: p1, processes: [ cmd: proc1 ] ], windows: [{length: 100, sc_partition: p1}]}" \
		   ">>> [error] Exception: Missing budget"
