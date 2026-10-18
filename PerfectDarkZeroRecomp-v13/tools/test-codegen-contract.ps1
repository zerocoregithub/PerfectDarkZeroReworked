param([Parameter(Mandatory)][string]$GeneratedDir)
$ErrorActionPreference='Stop'
$partition=Get-Content (Join-Path $GeneratedDir 'codegen.partition.json') -Raw|ConvertFrom-Json
if($partition.version -ne 2 -or $partition.file_count -ne 198){throw 'Unsupported generated partition layout'}
foreach($entry in @(@('827017F8',69),@('82685188',10),@('822397C8',112))) {
 if($partition.assignments.($entry[0]) -ne $entry[1]){throw "Function partition contract differs: $($entry[0])"}
}
$atomic=[IO.File]::ReadAllText((Join-Path $GeneratedDir 'perfectdarkzerorecomp_recomp.101.cpp')).Replace("`r`n","`n")
if($atomic.Contains('compare_exchange_strong(') -or
 !$atomic.Contains('ctx.cr0.eq = __sync_bool_compare_and_swap(') -or
 !$atomic.Contains('ctx.cr0.so = ctx.xer.so;') -or
 !$atomic.Contains("// mfmsr r8`n`tstd::atomic_thread_fence(std::memory_order_seq_cst);")) {
 throw 'Generator emission differs from accepted legacy atomic/condition-state contract; use the pinned legacy generator'
}
'PASS: representative layout and legacy atomic/CR/fence contract (not full semantic or gameplay proof)'
