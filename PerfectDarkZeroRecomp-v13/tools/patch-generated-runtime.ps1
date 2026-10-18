param([string]$GeneratedDir = (Join-Path $PSScriptRoot '..\generated\default'))
$ErrorActionPreference = 'Stop'
$pch = Join-Path $GeneratedDir 'perfectdarkzerorecomp_pch.h'
if (-not (Test-Path $pch)) { throw "Generated PCH not found: $pch" }
$text = [IO.File]::ReadAllText($pch)
$nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
$old = '#define REX_FUNC_PROLOGUE() __builtin_assume(((size_t)base & 0x1F) == 0)'
$new = '#define REX_FUNC_PROLOGUE() do { ctx.fpscr.InitHost(); __builtin_assume(((size_t)base & 0x1F) == 0); } while (0)'
$profileOld = "#define REX_FUNC_PROLOGUE()                     \$nl  __builtin_assume(((size_t)base & 0x1F) == 0); \$nl  ZoneNamedN(___tracy_guest_zone, __func__, TracyIsStarted)"
$profileNew = "#define REX_FUNC_PROLOGUE()                     \$nl  ctx.fpscr.InitHost();                         \$nl  __builtin_assume(((size_t)base & 0x1F) == 0); \$nl  ZoneNamedN(___tracy_guest_zone, __func__, TracyIsStarted)"
if ($text.Contains($old)) {
    $text = $text.Replace($old, $new)
} elseif (-not $text.Contains($new)) {
    throw 'Generated PCH function-prologue layout was not recognized.'
}
if ($text.Contains($profileOld)) {
    $text = $text.Replace($profileOld, $profileNew)
} elseif (-not $text.Contains($profileNew)) {
    throw 'Generated PCH profiling function-prologue layout was not recognized.'
}
if ([IO.File]::ReadAllText($pch) -cne $text) {
    [IO.File]::WriteAllText($pch, $text)
}
Write-Host 'Patched generated function prologues to mask host FP exceptions.'
