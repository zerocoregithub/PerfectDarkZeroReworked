$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$code=Get-Content "$root/generated/default/perfectdarkzerorecomp_recomp.112.cpp" -Raw
$hook='PDZ_ConstrainThirdPersonTarget(ctx, base, pdz_target_player, ctx.r27.u32);'
if([regex]::Matches($code,[regex]::Escape($hook)).Count -ne 1){throw 'One clearance hook required'}
if(!$code.Contains('const uint32_t pdz_target_player = ctx.r3.u32;')){throw 'Missing original owner'}
$at=$code.IndexOf($hook)
if($at -lt $code.IndexOf('loc_82239CF0:') -or $at -gt $code.IndexOf('PDZ_RecordCoverTarget(base, ctx.r27.u32);')){throw 'Wrong final-target stage'}
$source=Get-Content "$root/src/pdz_qol.cpp" -Raw
foreach($guard in @('PDZ_CopyThirdPersonClearanceEye(player,eye)','LocalPlayer(base)!=player','base[player+0x16B9]','PPCContext call=ctx','call.r3.u64=32')) {
 if(!$source.Contains($guard)){throw "Missing isolation guard: $guard"}
}
'PASS: clearance owner, final-target placement and isolation guards (structural only)'
