#!/usr/bin/env fish

set CMD_BASE "~/benchmarks/thermobench/build/benchmarks/coremark-8thr-long M4"
set LENGTH 1
set REPETITION_COUNT 50

# record baseline with fixed frequency for all 4 available frequencies
for i in (seq 0 (math REPETITION_COUNT - 1))
    for freq_i in 0 1 2 3
        set name "$LENGTH""ms_fixed""$freq_i""_$i"
        set cmd "$CMD_BASE >""$name.out"
        echo $name
        /demos/demos-sched -p imx8_fixed:$freq_i,$freq_i -C "
            windows: [{length: $LENGTH, slices: [{cpu: 0-3, sc_partition: SC1}]}]
            partitions: [{name: SC1, processes: [{budget: $LENGTH, cmd: '$cmd'}]}]
        "
    end
end

# record alternating between the lowest and all other frequencies
for i in (seq 0 (math REPETITION_COUNT - 1))
    for freq_i in 1 2 3
        set name "$LENGTH""ms_alternating0""$freq_i""_$i"
        set cmd "$CMD_BASE >""$name"".out"
        echo $name
        /demos/demos-sched -p imx8_alternating:0,$freq_i,3,3 -C "
            windows: [{length: $LENGTH, slices: [{cpu: 0-3, sc_partition: SC1}]}]
            partitions: [{name: SC1, processes: [{budget: $LENGTH, cmd: '$cmd'}]}]
        "
    end
end

# record alternating between the highest and all other frequencies
# switching 30 is the same as 03, it is measured just to sanity check that we get the same results,
#  and in the resulting dataset, both are combined to a single category
for i in (seq 0 (math REPETITION_COUNT - 1))
    for freq_i in 0 1 2
        set name "$LENGTH""ms_alternating3""$freq_i""_$i"
        set cmd "$CMD_BASE >""$name"".out"
        echo $name
        /demos/demos-sched -p imx_alternating:3,$freq_i,3,3 -C "
            windows: [{length: $LENGTH, slices: [{cpu: 0-3, sc_partition: SC1}]}]
            partitions: [{name: SC1, processes: [{budget: $LENGTH, cmd: '$cmd'}]}]
        "
    end
end
