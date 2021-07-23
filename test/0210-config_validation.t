#!/usr/bin/env bash
. testlib
plan_tests 14

# set fixed log level for config tests; otherwise if user would run something
#  like `SPDLOG_LEVEL=trace make test`, the output would include the logs and the tests would fail
export SPDLOG_LEVEL=warning

test_validation_pass() {
    local test_name=$1
    local cfg_in=$2

    echo "======== NEXT TEST =======================" >&2
    demos-sched -d -C "$cfg_in" >&2
    is $? 0 "$1"
}

test_validation_fail() {
    local test_name=$1
    local cfg_in=$2
    local error_like_expected=$3

    local exit_code
    local demos_output # `local` must be separate, otherwise it overwrites the exit code from DEmOS
    echo "======== NEXT TEST =======================" >&2
    demos_output=$(demos-sched -d -C "$cfg_in" 2>&1)
    exit_code=$?
    echo "$demos_output" >&2
    is $exit_code 1 "$1 - DEmOS should exit with code 1"
    like "$demos_output" "$error_like_expected" "$test_name - error output matches"
}

test_validation_fail "unknown partition" "{
    windows: [{length: 1, cpu: 0, sc_partition: NON_EXISTENT}],
    partitions: [],
}" "unknown partition 'NON_EXISTENT'"

test_validation_fail "overlapping CPU sets inside window" "{
    windows: [{length: 1, slices: [
        {cpu: 0, sc_partition: SC1},
        {cpu: 0, sc_partition: SC2},
    ]}],
    partitions: [
        {name: SC1, processes: {cmd: echo, budget: 1}},
        {name: SC2, processes: {cmd: echo, budget: 1}},
    ],
}" "overlapping CPU sets"

test_validation_fail "unfeasible per-process freq combination - A53" "{
    windows: [
        {length: 1, slices: [
            {cpu: 0, sc_partition: SC1},
            {cpu: 1, sc_partition: SC2},]}
    ],
    partitions: [
        {name: SC1, processes: [{cmd: echo, budget: 1, _a53_freq: 1}]},
        {name: SC2, processes: [{cmd: echo, budget: 1, _a53_freq: 0}]},
    ],
}" "window #0, between SC partitions on CPU cluster '0-3'."

test_validation_fail "unfeasible per-process freq combination - A72" "{
    windows: [
        {length: 1, slices: [
            {cpu: 4, sc_partition: SC1},
            {cpu: 5, sc_partition: SC2},]}
    ],
    partitions: [
        {name: SC1, processes: [{cmd: echo, budget: 1, _a72_freq: 1}]},
        {name: SC2, processes: [{cmd: echo, budget: 1, _a72_freq: 0}]},
    ],
}" "window #0, between SC partitions on CPU cluster '4,5'."

test_validation_fail "unfeasible per-process freq combination - BE" "{
    windows: [
        {length: 1, slices: [
            {cpu: 0, be_partition: BE1},
            {cpu: 1, be_partition: BE2},]}
    ],
    partitions: [
        {name: BE1, processes: [{cmd: echo, budget: 1, _a53_freq: 1}]},
        {name: BE2, processes: [{cmd: echo, budget: 1, _a53_freq: 0}]},
    ],
}" "window #0, between BE partitions on CPU cluster '0-3'."

test_validation_pass "collision between SC and BE is ignored" "{
    windows: [
        {length: 1, slices: [
            {cpu: 0, sc_partition: SC1},
            {cpu: 1, be_partition: BE1},]}
    ],
    partitions: [
        {name: SC1, processes: [{cmd: echo, budget: 1, _a53_freq: 0}]},
        {name: BE1, processes: [{cmd: echo, budget: 1, _a53_freq: 1}]},
    ],
}"

test_validation_pass "single partition requesting multiple freqs is feasible" "{
    windows: [
        {length: 1, slices: [{cpu: 0, sc_partition: SC1}]}
    ],
    partitions: [
        {name: SC1, processes: [
            {cmd: echo, budget: 1, _a53_freq: 0},
            {cmd: echo, budget: 1, _a53_freq: 1},
        ]}
    ],
}"

test_validation_pass "same frequency from multiple partitions is feasible" "{
    windows: [
        {length: 1, slices: [
            {cpu: 0, be_partition: SC1},
            {cpu: 1, be_partition: SC2},
        ]}
    ],
    partitions: [
        {name: SC1, processes: [{cmd: echo, budget: 1, _a53_freq: 0}]},
        {name: SC2, processes: [{cmd: echo, budget: 1, _a53_freq: 0}]},
    ],
}"

test_validation_pass "freqs on different clusters are feasible" "{
    windows: [
        {length: 1, slices: [
            {cpu: 0, be_partition: SC1},
            {cpu: 4, be_partition: SC2},
        ]}
    ],
    partitions: [
        {name: SC1, processes: [{cmd: echo, budget: 1, _a53_freq: 0}]},
        {name: SC2, processes: [{cmd: echo, budget: 1, _a72_freq: 1}]},
    ],
}"