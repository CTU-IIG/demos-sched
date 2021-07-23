$SSH_DIR = "~/benchmarks/results/cpufreq_latency_coremark"
$FREQS = @("600 MHz", "896 MHz", "1104 MHz", "1200 MHz") #a53
$FIG_TITLE = @"
Coremark – 4 threads (A53 cluster)
alternating frequencies in a 1ms interval
"@

. ..\fns.ps1
