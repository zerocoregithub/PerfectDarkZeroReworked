$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$source=Get-Content -Raw -LiteralPath (Join-Path $repo 'generated/default/perfectdarkzerorecomp_recomp.112.cpp')
$begin=$source.IndexOf('DEFINE_REX_FUNC(sub_822397C8) {')
$end=$source.IndexOf('DEFINE_REX_FUNC(', $begin+1)
if($begin -lt 0 -or $end -lt 0){throw 'Native target acquisition function missing'}
$body=$source.Substring($begin,$end-$begin)
$hook=$body.IndexOf('PDZ_AdjustCoverTargetRay(')
if($hook -lt 0 -or [regex]::Matches($body,'PDZ_AdjustCoverTargetRay\(').Count -ne 1){throw 'Missing or duplicate cover ray hook'}
if($hook -gt $body.IndexOf('sub_825A4990(ctx, base);') -or $hook -gt $body.IndexOf('sub_82252798(ctx, base);')){throw 'Cover ray adjustment bypasses native obstruction testing'}
if($hook -lt $body.IndexOf('sub_82644660(ctx, base);')){throw 'Cover ray adjustment is before unprojection'}
if(!$body.Contains('ctx.r1.u32 + 80, ctx.r1.u32 + 96, ctx.r27.u32, ctx.f27.f64, ctx.f28.f64, ctx.f29.f64')){throw 'Wrong native ray arguments or stale near-point registers'}
Write-Output 'PASS: one target acquisition hook after unprojection, before native obstruction and collision checks'
