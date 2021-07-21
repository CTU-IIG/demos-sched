#!/usr/bin/env fish

set CMD_BASE "~/benchmarks/thermobench/build/benchmarks/coremark-long"
set CPUS "4"
set LENGTH 1
set BASE_I 0
set REPETITION_COUNT 10


echo 1 >/sys/devices/system/cpu/cpu4/online
echo 1 >/sys/devices/system/cpu/cpu5/online

echo 0 >/sys/devices/system/cpu/cpu0/online
echo 0 >/sys/devices/system/cpu/cpu1/online
echo 0 >/sys/devices/system/cpu/cpu2/online
echo 0 >/sys/devices/system/cpu/cpu3/online


# record baseline with fixed frequency for all 4 available frequencies
for i in (seq $BASE_I (math $BASE_I + $REPETITION_COUNT - 1))
    for freq_i in 0 1 2 3
        set name "fixed""$freq_i""_$i"
        set cmd "$CMD_BASE >""$name.out"
        echo $name
        # pin demos to CPU 5
        /demos/demos-sched -p imx8_fixed:$freq_i,$freq_i -C "
            demos_cpu: 5
            windows: [{length: $LENGTH, slices: [{cpu: $CPUS, sc_partition: SC1}]}]
            partitions: [{name: SC1, processes: [{budget: $LENGTH, cmd: '$cmd'}]}]
        "
    end
end

# record alternating between the lowest and all other frequencies
for i in (seq $BASE_I (math $BASE_I + $REPETITION_COUNT - 1))
    for freq_i in 1 2 3
        set name "alternating0""$freq_i""_$i"
        set cmd "$CMD_BASE >""$name"".out"
        echo $name
        /demos/demos-sched -p imx8_alternating:3,3,0,$freq_i -C "
            demos_cpu: 5
            windows: [{length: $LENGTH, slices: [{cpu: $CPUS, sc_partition: SC1}]}]
            partitions: [{name: SC1, processes: [{budget: $LENGTH, cmd: '$cmd'}]}]
        "
    end
end

# record alternating between the highest and all other frequencies
for i in (seq $BASE_I (math $BASE_I + $REPETITION_COUNT - 1))
    for freq_i in 1 2
        set name "alternating3""$freq_i""_$i"
        set cmd "$CMD_BASE >""$name"".out"
        echo $name
        /demos/demos-sched -p imx8_alternating:3,3,3,$freq_i -C "
            demos_cpu: 5
            windows: [{length: $LENGTH, slices: [{cpu: $CPUS, sc_partition: SC1}]}]
            partitions: [{name: SC1, processes: [{budget: $LENGTH, cmd: '$cmd'}]}]
        "
    end
end
