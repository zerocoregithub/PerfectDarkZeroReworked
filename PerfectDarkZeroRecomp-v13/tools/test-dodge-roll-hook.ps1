$ErrorActionPreference = 'Stop'
$path = Join-Path $PSScriptRoot '../generated/default/perfectdarkzerorecomp_recomp.136.cpp'
$source = Get-Content -Raw -LiteralPath $path
$begin = $source.IndexOf('DEFINE_REX_FUNC(sub_826884C8) {')
$end = $source.IndexOf('DEFINE_REX_FUNC(', $begin + 1)
if ($begin -lt 0 -or $end -lt 0) { throw 'Alternate integrator missing.' }
$body = $source.Substring($begin, $end - $begin)
if (([regex]::Matches($body, 'PDZ_AdjustDodgeRollVelocity')).Count -ne 1) {
    throw 'Duplicate or missing roll boost.'
}
if ($body.IndexOf('PDZ_AdjustDodgeRollVelocity') -gt $body.IndexOf('sub_8268C7F0(ctx, base)')) {
    throw 'Roll boost incorrectly placed after collision.'
}
if ($body.IndexOf('ctx.f30.f64 /= pdz_roll_scale') -lt $body.IndexOf('sub_8268CD50(ctx, base)')) {
    throw 'Feedback normalization incorrectly placed before collision.'
}
if ($body.Contains('ctx.f29.f64 /= pdz_roll_scale') -or
    $body.Contains('ctx.f29.f64 *= pdz_roll_scale')) { throw 'Vertical velocity altered.' }
if (-not $body.Contains('loc_82688884:' + "`n" + "`tctx.f30.f64 *= pdz_roll_scale;")) {
    throw 'Native world-displacement bookkeeping restoration missing.'
}
'PASS: one boost before collision, normalized feedback afterward, vertical velocity untouched'
