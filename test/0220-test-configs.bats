#!/usr/bin/env bats
load testlib

# Test that config files that we ship in test_config can be at least
# parsed. Several of them are hardware dependent and cannot be
# executed on arbitrary hardware. Therefore, their automated testing
# is problematic.

# set fixed log level for config tests; otherwise if user would run something
#  like `SPDLOG_LEVEL=trace make test`, the output would include the logs and the tests would fail
export SPDLOG_LEVEL=warning

test_config() {
    if ! run -0 demos-sched -d -c "../test_config/$BATS_TEST_DESCRIPTION"; then
        echo "Failed command: $BATS_RUN_COMMAND"
        return 1
    fi
}

@test "CPU_switching.yaml" {
    test_config
}
@test "api_init_test.yaml" {
    test_config
}
@test "api_test-multiple_slices.yaml" {
    test_config
}
@test "api_test.yaml" {
    test_config
}
@test "be_partition_window_overflow.yaml" {
    test_config
}
@test "be_wait.yaml" {
    test_config
}
@test "environment_test.yaml" {
    test_config
}
@test "init_slice_cpuset_loading.yaml" {
    test_config
}
@test "jitter_process.yaml" {
    test_config
}
@test "more_partitions.yaml" {
    test_config
}
@test "multiple_processes.yaml" {
    test_config
}
@test "per_process_power_policy.yaml" {
    test_config
}
@test "per_process_power_policy2.yaml" {
    test_config
}
@test "per_process_power_policy_tx2.yaml" {
    test_config
}
@test "per_slice_power_policy.yaml" {
    test_config
}
