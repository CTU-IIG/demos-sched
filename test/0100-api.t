#!/usr/bin/env bash
. testlib
plan_tests 3

api-test
is $? 1 "Running outside of demos-sched fails"

# Check that demos schedules first two iterations of the sub-process
readarray -t lines < <(demos-sched -C "{
    windows: [ {length: 50, slices: [ { cpu: 0, sc_partition: SC1 }] } ],
    partitions: [ { name: SC1, processes: [ { cmd: api-test first second, budget: 500 } ] } ]
}")
echo PATH=$PATH >&2
is "${lines[0]}" "first" "first iteration"
is "${lines[1]}" "second" "second iteration"
