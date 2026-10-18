param([string]$Directory='profiles/stylized-research-20261006/dxbc-v7')
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
foreach($entry in @(@(483407,80,60),@(975007,160,120),@(5235903,1216,640))){
  if((($entry[0] -band 8191)+1) -ne $entry[1] -or ((($entry[0] -shr 13) -band 8191)+1) -ne $entry[2]){throw 'Captured fetch dimension mismatch'}
}
foreach($hash in @('edcce21f19a013d1','07e9288309420978')){
  foreach($mode in @('rtv','rov')){
    $path=Join-Path $repo "$Directory/$hash-$mode-stylized.txt"
    $code=Get-Content -LiteralPath $path -Raw
    if($code -notmatch 'add r\d+\.xyz, r0\.xyzx, l\(0\.000000, 0\.000000, 0\.000000, 0\.000000\)\s+mov r0\.xyz') {throw "Soft-glow flow mismatch: $path"}
    if($code -notmatch 'mul r\d+\.xyz, l\(0\.000000, 0\.000000, 0\.000000, 0\.000000\), CB2\[1\]\[2\]\.xxxx') {throw "Flare flow mismatch: $path"}
    if($code -notmatch 'add r\d+\.xyz, r\d+\.xyzx, r0\.xyzx') {throw "Scene output flow missing: $path"}
  }
}
Write-Output 'PASS: captured input dimensions; four composite variants suppress soft glow/flare while retaining scene output'
