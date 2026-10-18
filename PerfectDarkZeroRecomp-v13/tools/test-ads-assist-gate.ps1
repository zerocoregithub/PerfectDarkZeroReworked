$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$code=Get-Content "$root/src/pdz_qol.cpp" -Raw
$begin=$code.IndexOf('bool ScopeActive(uint8_t* base,uint32_t player) {')
$end=$code.IndexOf('bool DualHeld(', $begin)
$body=$code.Substring($begin,$end-$begin)
if($body.IndexOf('if(AimHeld(base,player)) return true;') -lt 0 -or
   $body.IndexOf('if(AimHeld(base,player)) return true;') -gt $body.IndexOf('const float blend=')){throw 'ADS gate must precede camera/FOV/blend tests'}
if($body -match 'pdz_tp_enable|pdz_classic_scope'){throw 'ADS assist gate must not depend on perspective/artwork'}
foreach($function in @('PDZ_ApplyAutoAimSetting','PDZ_AutoAimApplicationMode')) {
 $start=$code.IndexOf($function+'(')
 $next=$code.IndexOf('const bool scoped=ScopeActive(base,player);',$start)
 if($next -lt $start -or $next-$start -gt 1500){throw "Shared policy bypass: $function"}
}
'PASS: ADS blocks assist before FOV/blend tests; shared application gates and dual-wield-aware input retained (structural only)'
