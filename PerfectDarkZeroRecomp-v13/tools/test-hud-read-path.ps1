$ErrorActionPreference = 'Stop'
$source = Get-Content (Join-Path $PSScriptRoot '../src/pdz_qol.cpp') -Raw
# Structural regression guard: per-element native callbacks must never request
# the expensive ammo snapshot. Geometry tests cover the unchanged draw output.
$calls = [regex]::Matches($source, 'if\(!ReadClassicHud\(([^;\r\n]*?)\)(?=\)|\|\|)')
if ($calls.Count -ne 4) { throw 'Review new HUD reader callers before building.' }
if ($calls[0].Groups[1].Value -ne 'memory,live,false' -or
    $calls[1].Groups[1].Value -ne 'runtime?runtime->virtual_membase():nullptr,state,true' -or
    $calls[2].Groups[1].Value -ne 'base,live,false' -or
    $calls[3].Groups[1].Value -ne 'base,state,false') {
    throw 'Only the overlay may request full ammo data.'
}
$begin = $source.IndexOf('bool ReadClassicHud(')
$end = $source.IndexOf('PDZ_NativeHudScaleScope::', $begin)
$reader = $source.Substring($begin, $end - $begin)
if ($reader -match 'base\[player\s*\+\s*0x16A3u?\]') {
    throw 'HUD presentation must not fall back when gameplay input is locked.'
}
if ($reader -notmatch 'HudWord\(base,player\)!=0x820C93A0u' -or
    $reader -notmatch 'HudPointer\(controller,984\)' -or
    $reader -notmatch 'HudPointer\(view,400\)' -or
    $source -notmatch 'HudTime\(\)-last>250') {
    throw 'Keep live-player/view validation and native-HUD render heartbeat.'
}
$gate = $reader.IndexOf('if(!include_ammo) return true;')
if ($gate -lt 0 -or $gate -gt $reader.IndexOf('const auto read_ammo=') -or
    $gate -gt $reader.IndexOf('rex::memory::QueryProtect')) {
    throw 'Native eligibility must return before weapon access and page queries.'
}
Write-Host 'PASS: native HUD callbacks skip ammo/page queries; overlay retains full snapshot.'
