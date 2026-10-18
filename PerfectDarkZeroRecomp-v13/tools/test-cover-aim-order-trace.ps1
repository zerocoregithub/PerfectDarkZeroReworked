$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot
$source=Get-Content (Join-Path $repo 'src/pdz_qol.cpp') -Raw
$generated=Get-Content (Join-Path $repo 'generated/default/perfectdarkzerorecomp_recomp.112.cpp') -Raw
foreach($stage in @(0,4,6,3,5)) {
  $pattern="PDZ_TraceCoverAimOrder\(base, (?:ctx\.r31\.u32|0), $stage,"
  if([regex]::Matches($generated,$pattern).Count -ne 1){throw "Stage $stage must occur exactly once"}
}
if($generated.IndexOf('ctx.r31.u32, 0,') -ge $generated.IndexOf('PDZ_ApplySwayAim(base, ctx.r31')){throw 'Input order'}
if($generated.IndexOf('base, 0, 6,') -ge $generated.IndexOf('base, 0, 3,')){throw 'Collision branch order'}
if($generated.IndexOf('base, 0, 3,') -ge $generated.IndexOf('base, 0, 5,')){throw 'Return order'}
$start=$source.IndexOf('void PDZ_TraceCoverAimOrder(')
$end=$source.IndexOf('// Called before suppressing',$start)
$body=$source.Substring($start,$end-$start)
foreach($forbidden in @('store_and_swap','REX_STORE','reinterpret_cast','PDZ_Adjust')) {
  if($body.Contains($forbidden)){throw "Diagnostic contains mutation token $forbidden"}
}
foreach($required in @('PDZ_COVER_AIM_TRACE','now-start>20000','sequence>=3000','LocalPlayer(base)!=player','GuestReadable')) {
  if(!$body.Contains($required)){throw "Missing diagnostic bound: $required"}
}
Write-Output 'PASS: diagnostic hook uniqueness/order, opt-in bounds and no direct guest-write operations (structural checks only)'
