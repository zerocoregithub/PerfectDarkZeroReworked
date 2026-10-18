$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$generated=Join-Path $repo 'generated/default'
$sites=@(
  @('163','ctx.r21.u64 = PDZ_IdleFistVisibility(base, ctx.r31.u32, ctx.r21.u32);'),
  @('163','ctx.r22.u64 = PDZ_IdleFistVisibility(base, ctx.r31.u32, ctx.r22.u32);'),
  @('135','PDZ_AdjustWeaponBob(base, ctx.r31.u32, ctx.r30.u32, ctx.f25.f64, ctx.f26.f64);'),
  @('135','PDZ_AdjustUnarmedViewmodel(base, ctx.r31.u32, ctx.r28.u32, ctx.r1.u32 + 160);'),
  @('126','PDZ_ObserveUnarmedAction(base, ctx.r31.u32, pdz_requested_weapon_action, ctx.r3.u32);'),
  @('166','PDZ_ObserveUnarmedAction(base, ctx.r30.u32, pdz_requested_weapon_action, ctx.r3.u32);'),
  @('143','PDZ_RecordWeaponInput(base, pdz_input_state, pdz_input_user, ctx.r3.u32);')
)
foreach($site in $sites) {
  $text=Get-Content (Join-Path $generated "perfectdarkzerorecomp_recomp.$($site[0]).cpp") -Raw
  if([regex]::Matches($text,[regex]::Escape($site[1])).Count -ne 1) {
    throw "Missing or duplicated live hook: $($site[1])"
  }
  if($text.Contains('ctx.r17.u64 = PDZ_IdleFistVisibility')){throw 'Rejected attached-mesh hook still present'}
}
$before=@{}
Get-ChildItem -LiteralPath $generated -Filter '*.cpp' | ForEach-Object {
  $before[$_.FullName]=(Get-FileHash -LiteralPath $_.FullName).Hash
}
& (Join-Path $PSScriptRoot 'fix-unresolved-tail-jumps.ps1') -GeneratedDir $generated
foreach($path in $before.Keys) {
  if((Get-FileHash -LiteralPath $path).Hash -ne $before[$path]) {
    throw "Reapplication not idempotent: $path"
  }
}
$input=Get-Content (Join-Path $repo 'src/pdz_input.cpp') -Raw
foreach($fragment in @('const auto& flags=separate?menu_pad_flags:pad_flags;',
                       'if(user!=0||info.synthetic) return;',
                       'for(auto flag:menu_pad_flags) values.insert_or_assign')) {
  if(!$input.Contains($fragment)){throw "Input ownership/persistence guard missing: $fragment"}
}
Write-Output 'PASS: seven live hook sites unique; rejected attachment gate absent; reapplication idempotent; controller menu maps selected from physical input and persisted.'
