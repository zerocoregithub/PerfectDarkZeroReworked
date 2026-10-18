$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot
$code=Get-Content (Join-Path $repo 'generated/default/perfectdarkzerorecomp_recomp.169.cpp') -Raw
$source=Get-Content (Join-Path $repo 'src/pdz_qol.cpp') -Raw
$call='PDZ_ApplyLocktopusMouse(base, ctx.r31.u32, ctx.f13.f64, ctx.f12.f64, ctx.f29.f64);'
if([regex]::Matches($code,[regex]::Escape($call)).Count -ne 1){throw 'Locktopus hook count'}
$site=$code.IndexOf($call)
if($site -lt $code.IndexOf('loc_826398B8:') -or $site -gt $code.IndexOf('loc_82639950:')){throw 'Hook outside native analog read/evaluation'}
$start=$source.IndexOf('void PDZ_ApplyLocktopusMouse(')
$body=$source.Substring($start,$source.IndexOf('void PDZ_ApplyDirectMouse(',$start)-$start)
foreach($guard in @('0x820CEBE0u','HudWord(base,puzzle+32)==1','HudWord(base,item+92)==player','!pdz::UiMouseCaptured()','ReadCapturedMouseTotals')) {
 if(!$body.Contains($guard)){throw "Missing puzzle guard: $guard"}
}
foreach($write in @('store_and_swap','REX_STORE','reinterpret_cast')) {
 if($body.Contains($write)){throw "Unexpected shared guest write: $write"}
}
if($body.Contains('!base[player+0x16A3]')){throw 'Active puzzle must not be rejected by pause-like player flag'}
if(!$body.Contains('seconds>0&&seconds<=.25')){throw 'Missing advancing native simulation gate'}
$input=Get-Content (Join-Path $repo 'src/pdz_input.cpp') -Raw
if(!$input.Contains('if(PDZ_LocktopusInputActive()) return false;')){throw 'Missing scoped keyboard-menu routing exception'}
Write-Output 'PASS: unique puzzle-local analog hook, vtable/state/local-owner/UI guards, no shared guest-input writes (structural only)'
