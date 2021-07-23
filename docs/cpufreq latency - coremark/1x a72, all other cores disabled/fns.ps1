$SSH_DIR = "~/benchmarks/results/cpufreq_latency_coremark_singlecore"
$FREQS = @("600 MHz", "1056 MHz", "1296 MHz", "1596 MHz") #a72
$FIG_TITLE = @"
Coremark – single-threaded, running on CPU 4(an A72 core)
all other cores are disabled; alternating frequencies in a 1ms interval
"@

. ..\fns.ps1