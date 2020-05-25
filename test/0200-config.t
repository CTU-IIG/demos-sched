#!/bin/bash
. testlib
plan_tests 5

out=$(config-parsing-test -C "{
    windows: [ {length: 500, sc_partition: SC} ],
    partitions: [ {name: SC, processes: [{cmd: echo, budget:100}] }]
}")
out_cmp="partitions:
  - name: SC
    processes:
      - cmd: echo
        budget: 300
windows:
  - length: 500
    slices:
      - cpu: 0-7
        sc_partition: SC"
is "$out" "$out_cmp" "missing slice definition"

out=$(config-parsing-test -C "{
    windows: [ {length: 500, sc_partition: [{cmd: proc1, budget: 500}] } ]
}")
out_cmp="partitions:
  - name: anonymous_0
    processes:
      - cmd: proc1
        budget: 500
windows:
  - length: 500
    slices:
      - cpu: 0-7
        sc_partition: anonymous_0"
is "$out" "$out_cmp" "partition definition in window"

out=$(config-parsing-test -C "{
    windows: [ {length: 500, sc_partition: [{cmd: proc1}] } ]
}")
out_cmp="partitions:
  - name: anonymous_0
    processes:
      - cmd: proc1
        budget: 300
windows:
  - length: 500
    slices:
      - cpu: 0-7
        sc_partition: anonymous_0"
is "$out" "$out_cmp" "default budget"

out=$(config-parsing-test -C "{
    windows: [ {length: 500, sc_processes: proc} ]
}")
out_cmp="partitions:
  - name: anonymous_0
    processes:
      - cmd: proc
        budget: 300
windows:
  - length: 500
    slices:
      - cpu: 0-7
        sc_partition: anonymous_0"
is "$out" "$out_cmp" "Process as string"

out=$(config-parsing-test -C "{
    windows: [ {length: 500, sc_processes: [proc1, proc2]} ]
}")
out_cmp="partitions:
  - name: anonymous_0
    processes:
      - cmd: proc1
        budget: 300
      - cmd: proc2
        budget: 300
windows:
  - length: 500
    slices:
      - cpu: 0-7
        sc_partition: anonymous_0"
is "$out" "$out_cmp" "Processes as string"
