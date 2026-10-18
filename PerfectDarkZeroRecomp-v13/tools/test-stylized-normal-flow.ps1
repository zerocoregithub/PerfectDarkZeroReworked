param([string]$Directory='profiles/stylized-research-20261006/dxbc-v7',[int]$ExpectedNormals=122,[string]$Strength='0.020000')
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$count=0
foreach($file in Get-ChildItem (Join-Path $repo $Directory) -Filter '*-stylized.txt') {
  $text=Get-Content $file.FullName -Raw
  $literal="l($Strength, $Strength, 0.000000, 0.000000)"
  if(!$text.Contains($literal)){continue}
  $pattern='mul (r[0-9]+)\.xy, \1\.xyxx, l\(0\.020000, 0\.020000, 0\.000000, 0\.000000\)\s+mov (r[0-9]+)\.xy, \1\.xyxx\s+mul r[0-9]+\.x, \2\.x, -\2\.x'
  $pattern=$pattern.Replace('0\.020000',[regex]::Escape($Strength))
  if(![regex]::IsMatch($text,$pattern)){
    if($Strength -eq '0.000000'){continue} # Zero also appears in unrelated shader instructions.
    throw ('Flattened XY not consumed in normal reconstruction: '+$file.Name)
  }
  $count++
}
if($count -ne ($ExpectedNormals*2)){throw ('Expected '+$ExpectedNormals+' normals x two modes, got '+$count)}
'PASS: '+$count+' shader variants feed normal XY strength '+$Strength+' into original Z reconstruction'
