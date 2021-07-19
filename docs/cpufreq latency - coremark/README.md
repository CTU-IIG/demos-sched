# CPUFreq frequency switching latency

for the **NXP i.MX8QM** board

When the Linux kernel requests a CPU frequency change from the hardware, the change is not instantaneous. The estimated latency according to the official kernel device tree is 150 µs. This experiment measures the real latency of a CPU frequency switch.

## Methodology

Previous measurements suggested that the frequency switch is blocking, and the CPU core does not execute instructions until the new frequency is set.

<mark>Add a time diagram illustrating the switch</mark>

**Idea:** Run a CPU-intensive benchmark under DEmOS, periodically change the CPU frequency, measure the overhead and use it to estimate the switching latency.

The multi-threaded version of the [CoreMark](https://www.eembc.org/coremark/) benchmark was used for this measurement, which is part of the benchmark suite included with [Thermobench](https://github.com/CTU-IIG/thermobench). CoreMark result is the number of iterations per second on a set of computations.

The benchmark ran on the 4 cores in the A53 cluster of the CPU. A DEmOS configuration was used, with 1 ms time windows, and a frequency switch at the beginning of each time window.

For each pair of measured frequencies, an idealized CoreMark score was computed as an average of the scores reached when running on each of the two frequencies individually. The overhead on each iteration was then computed for each data point as `overhead = 1 / reached_score - 1 / expected_score`. Then, the frequency switching latency was estimated as `latency = overhead / number_of_switches_per_iteration`. In the evaluation script written in Python, the latency is computed as follows:

```python
expected_iters = (FIXED_AVG_ITERS_PER_SEC[cpu_frequency_1] + FIXED_AVG_ITERS_PER_SEC[cpu_frequency_2) / 2
expected_period = 1 / expected_iters
iterations_per_second = # ... read from the benchmark output
periods = [1/i for i in iterations_per_second]
latencies = [(p - expected_period) / (p / WINDOW_LENGTH) for p in periods]
```

## Implementation

The `record.fish` script in the current directory runs the benchmarks 50 times for each tested pair of frequencies, and separately for each available frequency to provide a baseline.

The resulting dataset is then parsed using the PowerShell `csv` function inside `fns.ps1`, which creates a `dataset.csv` file. This file is then loaded in `plot.py`, which computes the latency estimate and plots it using *Matplotlib*. 3 charts are rendered: iterations per second, period (duration of each iteration) and switching latency.

## Results

### Iterations per second

The red lines above the violin plots for alternating CPU frequencies represent the idealized estimate averaged from the baseline measurements with fixed CPU frequency.

![Chart of iterations per second, including the baseline measurements with fixed frequency](./charts/iterations_per_second.svg)

### Iteration periods (average duration)

By inverting the *iterations per second* shown in the previous chart, we get the average duration of each iteration (here, the baseline measurements are omitted):

![Chart of iteration periods (average iteration duration)](./charts/iteration_period.svg)

### Latency estimate

Using the formula described above, the following latency estimates are calculated:

![Chart of latency estimates](./charts/latency.svg)

## Discussion

The CPU frequency switching latency varies depending on how many frequencies are skipped. For a switch between neighboring frequencies (e.g. 600 MHz and 896 MHz), the latency estimate approximately **matches the 150 µs figure** provided in the official device tree. A switch between the lowest and the highest frequency (600 MHz to 1200 MHz) takes around **200 µs** to complete.
