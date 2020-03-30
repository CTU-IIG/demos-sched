#!/bin/bash
. testlib
plan_tests 2

out=$(demos-sched -h)
is $? 0 "exit code"
like "$(head -n1 <<<$out)" "Usage: demos-sched" "help printed"
