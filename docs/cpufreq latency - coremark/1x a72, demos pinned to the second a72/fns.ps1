$SSH_DIR = "~/benchmarks/results/cpufreq_latency_1x_a72_pinned"
$FREQS = @("600 MHz", "1056 MHz", "1296 MHz", "1596 MHz") #a72
$FIG_TITLE = @"
Coremark – single-threaded, running on CPU 4 (an A72 core)
DEmOS process is pinned to CPU 5 (also an A72 core)
alternating frequencies in a 1ms interval
"@

. ..\fns.ps1
