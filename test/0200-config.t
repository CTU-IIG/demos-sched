#!/bin/bash
. testlib
plan_tests 5

out=$(demos-sched -C "{
    windows: [ {length: 500, sc_partition: SC} ],
    partitions: [ {name: SC, processes: [{cmd: echo, budget:100}] }]
}" -d)
out_cmp="partitions:
  - name: SC
    processes:
      - cmd: echo
        budget: 300
windows:
  - length: 500
    slices:
      - cpu: 0-63
        sc_partition: SC"
is "$out" "$out_cmp" "missing slice definition"

out=$(demos-sched -C "{
    windows: [ {length: 500, sc_partition: [{cmd: proc1, budget: 500}] } ]
}" -d)
out_cmp="partitions:
  - name: anonymous_0
    processes:
      - cmd: proc1
        budget: 500
windows:
  - length: 500
    slices:
      - cpu: 0-63
        sc_partition: anonymous_0"
is "$out" "$out_cmp" "partition definition in window"

out=$(demos-sched -C "{
    windows: [ {length: 500, sc_partition: [{cmd: proc1}] } ]
}" -d)
out_cmp="partitions:
  - name: anonymous_0
    processes:
      - cmd: proc1
        budget: 300
windows:
  - length: 500
    slices:
      - cpu: 0-63
        sc_partition: anonymous_0"
is "$out" "$out_cmp" "default budget"

out=$(demos-sched -C "{
    windows: [ {length: 500, sc_processes: proc} ]
}" -d)
out_cmp="partitions:
  - name: anonymous_0
    processes:
      - cmd: proc
        budget: 300
windows:
  - length: 500
    slices:
      - cpu: 0-63
        sc_partition: anonymous_0"
is "$out" "$out_cmp" "Process as string"

out=$(demos-sched -C "{
    windows: [ {length: 500, sc_processes: [proc1, proc2]} ]
}" -d)
out_cmp="partitions:
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
is "$out" "$out_cmp" "Processes as string"
