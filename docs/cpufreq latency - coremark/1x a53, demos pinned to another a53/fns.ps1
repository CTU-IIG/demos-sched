$SSH_DIR = "~/benchmarks/results/cpufreq_latency_1x_a53_pinned"
$FREQS = @("600 MHz", "896 MHz", "1104 MHz", "1200 MHz") #a53
$FIG_TITLE = @"
Coremark – single-threaded, running on CPU 0 (A53 core)
DEmOS process is pinned to CPU 1 (A53 core)
alternating frequencies in a 1ms interval
"@

. ..\fns.ps1
