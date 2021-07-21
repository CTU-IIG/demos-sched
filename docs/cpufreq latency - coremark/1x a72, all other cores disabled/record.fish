#!/usr/bin/env fish

set CMD_BASE "~/benchmarks/thermobench/build/benchmarks/coremark-long"
set CPUS "4"
set LENGTH 1
set BASE_I 0
set REPETITION_COUNT 10

# keep online only core 4 (one of the A72 cores)
echo 0 >/sys/devices/system/cpu/cpu0/online
echo 0 >/sys/devices/system/cpu/cpu1/online
echo 0 >/sys/devices/system/cpu/cpu2/online
echo 0 >/sys/devices/system/cpu/cpu3/online
echo 1 >/sys/devices/system/cpu/cpu4/online
echo 0 >/sys/devices/system/cpu/cpu5/online

# record baseline with fixed frequency for all 4 available frequencies
for i in (seq $BASE_I (math $BASE_I + $REPETITION_COUNT - 1))
    for freq_i in 0 1 2 3
        set name "$LENGTH""ms_a72_fixed""$freq_i""_$i"
        set cmd "$CMD_BASE >""$name.out"
        echo $name
        /demos/demos-sched -p imx8_fixed:$freq_i,$freq_i -C "
            windows: [{length: $LENGTH, slices: [{cpu: $CPUS, sc_partition: SC1}]}]
            partitions: [{name: SC1, processes: [{budget: $LENGTH, cmd: '$cmd'}]}]
        "
    end
end

# record alternating between the lowest and all other frequencies
for i in (seq $BASE_I (math $BASE_I + $REPETITION_COUNT - 1))
    for freq_i in 1 2 3
        set name "$LENGTH""ms_a72_alternating0""$freq_i""_$i"
        set cmd "$CMD_BASE >""$name"".out"
        echo $name
        /demos/demos-sched -p imx8_alternating:3,3,0,$freq_i -C "
            windows: [{length: $LENGTH, slices: [{cpu: $CPUS, sc_partition: SC1}]}]
            partitions: [{name: SC1, processes: [{budget: $LENGTH, cmd: '$cmd'}]}]
        "
    end
end

# record alternating between the highest and all other frequencies
# switching 30 is the same as 03, it is measured just to sanity check that we get the same results,
#  and in the resulting dataset, both are combined to a single category
for i in (seq $BASE_I (math $BASE_I + $REPETITION_COUNT - 1))
    for freq_i in 0 1 2
        set name "$LENGTH""ms_a72_alternating3""$freq_i""_$i"
        set cmd "$CMD_BASE >""$name"".out"
        echo $name
        /demos/demos-sched -p imx8_alternating:3,3,3,$freq_i -C "
            windows: [{length: $LENGTH, slices: [{cpu: $CPUS, sc_partition: SC1}]}]
            partitions: [{name: SC1, processes: [{budget: $LENGTH, cmd: '$cmd'}]}]
        "
    end
end
