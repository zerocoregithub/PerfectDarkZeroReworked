# Static checks only. Does not compile, run the game, or rewrite generated code.
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$header = [IO.File]::ReadAllText((Join-Path $repo 'src/pdz_door_autoclose.h'))
$entries = [regex]::Matches($header,
    '\{\{\{0x([0-9A-F]{8}), 0x([0-9A-F]{8}), 0x([0-9A-F]{8})\}\}, (kWooden|kRusty)\}')
if ($entries.Count -ne 7) { throw 'Expected seven narrowly identified door instances' }
$definitions = @(& (Join-Path $PSScriptRoot 'inspect-door-definitions.ps1'))
$targets = @($definitions | Where-Object {
    $_.Package -eq '1959974102.pkg' -and $_.TimerEnabled -and
    $_.Model -cin @('aid_darkcharacter_xenon_props_door_wooden_door',
                   'aid_darkcharacter_xenon_props_door_Rusty_door')
})
if ($targets.Count -ne 7) { throw 'Shipped warehouse definition selection changed' }
foreach ($entry in $entries) {
    $bits = ($entry.Groups[1].Value, $entry.Groups[2].Value, $entry.Groups[3].Value) -join ','
    $model = if ($entry.Groups[4].Value -eq 'kWooden') {
        'aid_darkcharacter_xenon_props_door_wooden_door'
    } else { 'aid_darkcharacter_xenon_props_door_Rusty_door' }
    $match = @($targets | Where-Object { $_.PositionBits -ceq $bits -and $_.Model -ceq $model })
    if ($match.Count -ne 1) { throw "Door signature does not uniquely match shipped data: $bits" }
    $all = @($definitions | Where-Object { $_.PositionBits -ceq $bits -and $_.Model -ceq $model })
    if ($all.Count -ne 1) { throw 'Door signature overlaps another inspected definition' }
}
$generated = [IO.File]::ReadAllText((Join-Path $repo 'generated/default/perfectdarkzerorecomp_recomp.126.cpp')).Replace("`r`n", "`n")
$patch = "`tctx.lr = 0x82552714;`n`tif (PDZ_AllowTimedDoorClose(base, ctx.r31.u32)) {`n`t`tsub_82553640(ctx, base);`n`t}`nloc_82552714:"
if (-not $generated.Contains($patch) -or
    ([regex]::Matches($generated, 'PDZ_AllowTimedDoorClose\(')).Count -ne 1 -or
    -not $generated.Contains('#include "../../src/pdz_qol.h"')) {
    throw 'Generated gate must wrap exactly the proven timer call, with helper declaration included'
}
$errors = $null
$tokens = $null
[Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $PSScriptRoot 'fix-unresolved-tail-jumps.ps1'), [ref]$tokens, [ref]$errors) | Out-Null
if ($errors.Count) { throw ($errors | Out-String) }
'PASS: seven exact shipped door signatures; one timer-call gate; repair script parses. No build performed.'
