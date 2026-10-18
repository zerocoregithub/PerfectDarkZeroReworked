$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
foreach($relative in @('src/pdz_qol.cpp','src/pdz_qol.h','src/pdz_overlay.cpp','src/mod/third_person.cpp','generated/default/perfectdarkzerorecomp_recomp.112.cpp')) {
 $text=Get-Content (Join-Path $root $relative) -Raw
 if($text -match 'TPPOSE|TPAIM|pdz_tp_aim_trace|TraceThirdPerson|StartThirdPersonAimTrace'){throw "Retired diagnostic remains: $relative"}
}
$qol=Get-Content "$root/src/pdz_qol.cpp" -Raw
foreach($required in @('PDZ_ConstrainThirdPersonTarget','PDZ_ReadThirdPersonImpact','ObserveDodgeRoll(base,player,config,seconds);')) {
 if(!$qol.Contains($required)){throw "Accepted functionality missing: $required"}
}
$app=Get-Content "$root/src/perfectdarkzerorecomp_app.h" -Raw
$load=$app.IndexOf('static void LoadSettingsFiles()')
$reset=$app.IndexOf('SetFlagByName("pdz_tp_enable", "false")',$load)
if($reset -lt $load -or $reset -lt $app.IndexOf('rex::cvar::LoadConfig(p);',$load)){throw 'First-person startup reset must follow saved configuration load'}
$camera=Get-Content "$root/src/mod/third_person.cpp" -Raw
if(!$camera.Contains('RegisterBind("bind_third_person", "F7"')){throw 'Perspective switching removed'}
'PASS: retired TP tracing absent; accepted hooks retained; first-person startup follows config load (structural only)'
