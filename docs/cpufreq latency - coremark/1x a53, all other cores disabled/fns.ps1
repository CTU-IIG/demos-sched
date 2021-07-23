$SSH_DIR = "~/benchmarks/results/cpufreq_latency_1x_a53"
$FREQS = @("600 MHz", "896 MHz", "1104 MHz", "1200 MHz") #a53
$FIG_TITLE = @"
Coremark – single-threaded, running on CPU 0 (an A53 core)
all other cores are disabled; alternating frequencies in a 1ms interval
"@

. ..\fns.ps1
