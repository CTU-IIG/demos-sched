#!/bin/bash
. testlib
plan_tests 7

out=$(demos-sched -C "{ windows: [], partitions: [], garbage: garbage}" 2>&1)
is $? 1 "garbage causes failure"
like "$out" garbage "garbage reported"

out=$(demos-sched -C "{
    windows: [ {length: 500, sc_partition: SC} ],
    partitions: [ {name: SC, processes: [{cmd: echo, budget: 100}] }]
}" -d)
expected="partitions:
  - name: SC
    processes:
      - cmd: echo
        budget: 100
windows:
  - length: 500
    slices:
      - cpu: 0-63
        sc_partition: SC"
is "$out" "$expected" "missing slice definition"

out=$(demos-sched -C "{
    windows: [ {length: 500, sc_partition: [{cmd: proc1, budget: 500}] } ]
}" -d)
expected="partitions:
  - name: anonymous_0
    processes:
      - cmd: proc1
        budget: 500
windows:
  - length: 500
    slices:
      - cpu: 0-63
        sc_partition: anonymous_0"
is "$out" "$expected" "partition definition in window"

out=$(demos-sched -C "{
    windows: [ {length: 500, sc_partition: [{cmd: proc1}] } ]
}" -d)
expected="partitions:
  - name: anonymous_0
    processes:
      - cmd: proc1
        budget: 300
windows:
  - length: 500
    slices:
      - cpu: 0-63
        sc_partition: anonymous_0"
is "$out" "$expected" "default budget"

# This is the case, why we need sc_processes and cannot reuse
# sc_partition here.
out=$(demos-sched -C "{
    windows: [ {length: 500, sc_processes: proc} ]
}" -d)
expected="partitions:
  - name: anonymous_0
    processes:
      - cmd: proc
        budget: 300
windows:
  - length: 500
    slices:
      - cpu: 0-63
        sc_partition: anonymous_0"
is "$out" "$expected" "Process as string"

out=$(demos-sched -C "{
    windows: [ {length: 500, sc_processes: [proc1, proc2]} ]
}" -d)
expected="partitions:
  - name: anonymous_0
    processes:
      - cmd: proc1
        budget: 150
      - cmd: proc2
        budget: 150
windows:
  - length: 500
    slices:
      - cpu: 0-63
        sc_partition: anonymous_0"
is "$out" "$expected" "Processes as string"
