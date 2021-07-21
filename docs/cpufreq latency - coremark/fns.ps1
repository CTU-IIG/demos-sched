if (-not (Get-Variable SSH_DIR -ErrorAction Ignore)) {
	throw "`$SSH_DIR not set."
}
if (-not (Get-Variable FIG_TITLE -ErrorAction Ignore)) {
	throw "`$FIG_TITLE not set."
}

$SSH_HOST = "fel_imx8b"


function upload {
	ssh $SSH_HOST "mkdir --parents $SSH_DIR"
	scp ./record.fish ${SSH_HOST}:$SSH_DIR/record.fish
	ssh $SSH_HOST "chmod +x $SSH_DIR/record.fish"
}

function record {
	ssh -t $SSH_HOST "cd $SSH_DIR && ./record.fish"
}

function download {
	scp -r ${SSH_HOST}:$SSH_DIR/*.out .\dataset
}

function sort-files {
	ls -File .\dataset | % {
		$n = ".\dataset\" + $_.Name.Substring(0, $_.Name.LastIndexOf("_"))
		$null = mkdir -Force $n
		mv $_ .\$n
	}
}

function csv {
	ls .\dataset -Directory | ? {-not $_.Name.StartsWith("_")} | % {
		Select-String -Path $_\* -Pattern "Iterations/Sec" -Raw
			| % {($_ -split ":")[1].Trim()}
			| Join-String -Separator "," -OutputPrefix ($_.Name + ",")
	} >.\dataset.csv
	
	ls .\dataset -Directory -Filter "*fixed*" | % {
		Select-String -Path $_\* -Pattern "Iterations/Sec" -Raw
			| % {($_ -split ":")[1].Trim()}
			| measure -Average
	} | Join-String Average "," >.\fixed_averages.csv
}

function plot {
	py $PSScriptRoot\plot.py $FIG_TITLE @Args
}