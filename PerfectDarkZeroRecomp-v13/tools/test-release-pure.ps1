param([string]$Compiler='clang++',
      [string]$OutputDirectory='out/release-pure',
      [string]$SDKSource=(Join-Path $PSScriptRoot '../../rexglue-sdk-v13'))
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $repo
try {
  $output=[IO.Path]::GetFullPath($OutputDirectory)
  New-Item -ItemType Directory -Path $output -Force | Out-Null
  $env:TEMP=Join-Path $output 'temp';$env:TMP=$env:TEMP
  New-Item -ItemType Directory -Path $env:TEMP -Force | Out-Null
  $sdk=(Resolve-Path -LiteralPath $SDKSource).Path
  $cases=@('aim-retention','aim-assist-policy','ammo-visibility','camspy-mouse','classic-aim',
    'classic-hud','classic-scope','cover-camera-anchor','cover-reprojection',
    'cover-target-ray','cover-view-math','display-mode','dodge-roll-math',
    'door-autoclose','fp-helper-policy','gyro','input-math','locktopus-mouse',
    'movement-math','npc-damage-math','presets','reload-interact','stick-curves',
    'unarmed-controls','weapon-accuracy','weapon-controls','weapon-sway','weapon-timing')
  $results=[Collections.Generic.List[object]]::new()
  foreach($case in $cases) {
    $exe=Join-Path $output ("test-$case.exe")
    & $Compiler -std=c++23 -UNDEBUG "-I$sdk/include" "-I$sdk/thirdparty/imgui" "tools/test-$case.cpp" -o $exe
    if($LASTEXITCODE -ne 0){throw "Compile failed: $case"}
    $lines=@(& $exe)
    if($LASTEXITCODE -ne 0){throw "Test failed: $case"}
    $results.Add([pscustomobject]@{Test=$case;Result='PASS';Evidence=$lines -join ' ';ExeSHA256=(Get-FileHash $exe).Hash})
    $lines
  }
  $results | Export-Csv (Join-Path $output 'results.csv') -NoTypeInformation
  "PASS: $($results.Count) freshly compiled algorithm tests; assertions enabled; no game/device launch"
} finally {Pop-Location}
