$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$image = [IO.File]::ReadAllBytes((Join-Path $repo 'profiles/stylized-research-20261006/offline-image/default-image.bin'))
$expected = [byte[]](0x80,0x63,0x05,0x68,0x81,0x63,0,0,0x81,0x6B,0,0x10,0x7D,0x69,3,0xA6,0x4E,0x80,4,0x20)
for ($i=0; $i -lt $expected.Length; ++$i) {
  if ($image[0x5A8080+$i] -ne $expected[$i]) { throw 'Original thunk byte mismatch' }
}
$source = [IO.File]::ReadAllText((Join-Path $repo 'src/perfectdarkzerorecomp_app.h'))
foreach ($instruction in @('ctx.r3.u64 = load(ctx.r3.u32 + 1384);',
    'ctx.r11.u64 = load(ctx.r3.u32);', 'ctx.r11.u64 = load(ctx.r11.u32 + 16);',
    'ctx.ctr.u64 = ctx.r11.u64;', 'rex::runtime::ResolveIndirectFunction(ctx.ctr.u32)(ctx, memory);')) {
  if (!$source.Contains($instruction)) { throw "Missing original operation: $instruction" }
}
if (!$source.Contains('!dispatcher->GetFunction(0x825A8080)') -or
    !$source.Contains('std::memcmp(base + 0x825A8080, expected, sizeof(expected))')) {
  throw 'Missing registration/image guards'
}
$boundaries = [IO.File]::ReadAllText((Join-Path $repo 'default_functions.toml'))
if (!$boundaries.Contains('0x825A8080 = {}')) { throw 'Future codegen boundary missing' }
'PASS: all five original PPC instructions, exact offline-image bytes, guarded registration, future codegen boundary'
