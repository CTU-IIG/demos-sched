function download($NamePattern, $Dir) {
	$null = mkdir -Force .\dataset\$Dir
	scp -r fel_imx8:~/benchmarks/results/cpufreq_latency_coremark/$NamePattern .\dataset\$Dir
}

function sort-files {
	ls -File | % {
		$n = $_.Name.Substring(0, $_.Name.LastIndexOf("_"))
		$null = mkdir -Force $n
		mv $_ .\$n
	}
}

function stats {
	ls .\dataset -Directory | ? {-not $_.Name.StartsWith("_")} | % {
		$avg = Select-String -Path $_\* -Pattern "Iterations/Sec" -Raw
			| % {($_ -split ":")[1].Trim()}
			| measure -Average
		[pscustomobject]@{Name=$_.Name; Average=$avg.Average}
	}
}

function csv {
	ls .\dataset -Directory | ? {-not $_.Name.StartsWith("_")} | % {
		Select-String -Path $_\* -Pattern "Iterations/Sec" -Raw
			| % {($_ -split ":")[1].Trim()}
			| Join-String -Separator ";" -OutputPrefix ($_.Name + ";")
	} > .\dataset.csv
}
