param([string]$GeneratedDir = (Join-Path $PSScriptRoot '..\generated\default'))
$ErrorActionPreference = 'Stop'
$dir = (Resolve-Path -LiteralPath $GeneratedDir).Path
$files = Get-ChildItem -LiteralPath $dir -Filter '*recomp.*.cpp'
foreach ($file in $files) {
  $text = [IO.File]::ReadAllText($file.FullName)
  if (-not $text.Contains('DEFINE_REX_FUNC(sub_822074C8)')) { continue }
  # Migrate historical generated output: the retired identity function returned
  # its argument unchanged. Preserve the native mode load and optional probe.
  if (-not $text.Contains('ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 732);')) { throw 'Vision selection site changed' }
  $clean = [regex]::Replace($text,'(?m)^\s*extern "C" unsigned int PDZ_VisionColorMode\(unsigned int\);\r?\n','')
  $clean = [regex]::Replace($clean,'(?m)^\s*ctx.r11.u64 = PDZ_VisionColorMode\(ctx.r11.u32\);\r?\n','')
  if ($clean -cne $text) { [IO.File]::WriteAllText($file.FullName,$clean) }
  return
}
throw 'Vision selection function not found'
