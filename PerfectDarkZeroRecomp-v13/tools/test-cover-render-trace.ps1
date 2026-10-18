$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$source=Get-Content -Raw -LiteralPath (Join-Path $repo 'src/pdz_qol.cpp')
$start=$source.IndexOf('// Opt-in diagnostic only:')
if($start -lt 0){throw 'Diagnostic section missing'}
$trace=$source.Substring($start)
foreach($name in @('827017F8','826DA3D8')) {
  if([regex]::Matches($trace,"REX_HOOK_RAW\(sub_$name\)").Count -ne 1){throw 'Duplicate/missing raw hook'}
  if([regex]::Matches($trace,"__imp__sub_$name\(ctx,\s*base\)").Count -ne 1){throw 'Native call not preserved exactly once'}
}
foreach($required in @('pdz_cover_render_trace, false','caller==0x82407058u','caller==0x824070A0u','caller==0x824072E8u','ctx.r29.u32:ctx.r30.u32','sequence<64','cover_trace_draws.fetch_add(1)<256','cover_draw_diagnostic=saved','GuestReadable(base,render,20460)')) {
  if(!$trace.Contains($required)){throw "Missing guard: $required"}
}
if($trace -match 'ctx\.[A-Za-z0-9_.]+\s*=(?!=)|store_and_swap|WriteGuest|Protect\(|base\[[^\]]+\]\s*=(?!=)') {
  throw 'Diagnostic contains guest register/memory write'
}
Write-Output 'PASS: opt-in bounded attribution, preserved native calls, context restoration, no diagnostic guest writes'
Write-Output 'LIMIT: structural source check only; GPU attribution and disabled runtime equivalence need capture/playtest'
