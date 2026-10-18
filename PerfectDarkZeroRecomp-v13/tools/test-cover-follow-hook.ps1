$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot
$code=Get-Content (Join-Path $repo 'generated/default/perfectdarkzerorecomp_recomp.112.cpp') -Raw
$source=Get-Content (Join-Path $repo 'src/pdz_qol.cpp') -Raw
if([regex]::Matches($code,'PDZ_RecordCoverTarget\(base, ctx.r27.u32\);').Count -ne 1){throw 'Target hook must occur once'}
if($code.IndexOf('PDZ_RecordCoverTarget(base') -lt $code.IndexOf('loc_82239CF0:')){throw 'Target hook before resolved point'}
if($code.IndexOf('PDZ_RecordCoverTarget(base') -gt $code.IndexOf('ctx.lr = 0x82239DC4;')){throw 'Target hook after return output'}
$start=$source.IndexOf('void PDZ_RecordCoverTarget(')
$end=$source.IndexOf('void PDZ_TraceCoverAimOrder(',$start)
$body=$source.Substring($start,$end-$start)
foreach($token in @('store_and_swap','REX_STORE','reinterpret_cast')) {
  if($body.Contains($token)){throw "Target observer writes guest state: $token"}
}
foreach($token in @('LocalPlayer(base)!=cover_acquisition.player','now-cover_acquisition.time>100','cover_composed_state.branch!=0','cover_target_camera.aim_camera!=cover_acquisition.aim_camera')) {
  if(!$body.Contains($token)){throw "Missing ownership/freshness guard: $token"}
}
if(!$source.Contains('pdz::SetCoverViewEye(cover_acquisition.aim_view,camera.eye)')){throw 'Missing virtual aim view'}
if(!$source.Contains('cover_anchor.follow_weight=0;')){throw 'Missing invalid/near target follow fallback'}
Write-Output 'PASS: resolved-target hook placement, read-only observer, owner/freshness guards and virtual-aim/fallback wiring (structural only)'
