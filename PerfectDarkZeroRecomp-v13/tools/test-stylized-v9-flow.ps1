param([string]$Revision='v9')
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$inventory=Get-Content (Join-Path $repo '../rexglue-sdk-v13/include/rex/graphics/pdz_material_inventory.h') -Raw
$programs=[regex]::Matches($inventory,'\{0x([A-F0-9]{16}), (\d+), (\d+), (true|false)\}')
$folders=@("$Revision-cache-validation","$Revision-package-validation")
$verified=0;$parallax=0
foreach($m in $programs){
  $hash=$m.Groups[1].Value;$found=$false
  foreach($folder in $folders){
    foreach($mode in @('rtv','rov')){
      $path=Join-Path $repo "profiles/stylized-research-20261006/$folder/$hash-$mode-stylized.txt"
      if(!(Test-Path -LiteralPath $path)){continue}
      $found=$true;$s=Get-Content -LiteralPath $path -Raw
      # The banding starts by copying guest r0 (lighting), not r1 (albedo).
      if($s -notmatch 'mov r\d+\.xyz, r0\.xyzx\s+max r\d+\.x') {throw "Lighting operand flow: $hash"}
      if($Revision -eq 'v9'){
        if($s -notmatch 'mul r\d+\.xyz, r1\.xyzx, l\(0\.650000') {throw "Albedo contrast flow: $hash"}
      }else{
        if($s -notmatch 'mul r\d+\.xyz, r1\.xyzx, l\(0\.212600, 0\.715200, 0\.072200' -or
           $s -notmatch 'mul r\d+\.xyz, r\d+\.xyzx, l\(1\.120000' -or $s -match 'l\(0\.175000') {throw "Chroma/gray removal flow: $hash"}
      }
      if($m.Groups[4].Value -eq 'true'){
        if($s -notmatch 'mul r\d+\.xy, l\(0\.000000, 0\.000000, 0\.000000, 0\.000000\), r2\.wwww') {throw "Parallax offset flow: $hash"}
      }
    }
  }
  if(!$found){throw "Untested material: $hash"}
  $verified++;if($m.Groups[4].Value -eq 'true'){$parallax++}
}
if($verified -ne 322 -or $parallax -ne 67){throw 'Inventory coverage mismatch'}
foreach($hash in @('edcce21f19a013d1','07e9288309420978')){
  foreach($mode in @('rtv','rov')){
    $s=Get-Content (Join-Path $repo "profiles/stylized-research-20261006/$Revision-cache-validation/$hash-$mode-stylized.txt") -Raw
    if($s -notmatch 'mul r\d+\.xyz, r1\.xyzx, l\(0\.150000' -or $s -notmatch 'mul r\d+\.xyz, r2\.xyzx, l\(0\.150000'){throw 'Glow restoration flow mismatch'}
  }
}
"PASS: $verified unique material programs; $parallax height-offset removals; lighting/albedo operands and 15% composite glow verified"
