param([string]$Fxc='C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/fxc.exe',
      [string]$OutputDirectory='')
$ErrorActionPreference='Stop'
$shaderRoot=Join-Path (Split-Path $PSScriptRoot) 'src/ui/shaders'
if(!$OutputDirectory){$OutputDirectory=Join-Path $shaderRoot 'bytecode/d3d12_5_1'}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
foreach($shader in @(
  @('FullscreenVS','vs_5_1','pdz_hdr_fullscreen_vs'),
  @('OutputPS','ps_5_1','pdz_hdr_output_ps'),
  @('OverlayPS','ps_5_1','pdz_hdr_overlay_ps'))) {
  & $Fxc /nologo /WX /O3 /T $shader[1] /E $shader[0] /Vn $shader[2] /Fh "$OutputDirectory/$($shader[2]).h" "$shaderRoot/pdz_hdr.hlsl"
  if($LASTEXITCODE){throw "HDR shader compilation failed: $($shader[0])"}
}
