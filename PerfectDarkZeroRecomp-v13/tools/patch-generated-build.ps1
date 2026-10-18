param([Parameter(Mandatory)][string]$GeneratedDir)
$ErrorActionPreference='Stop'
$cmake=Join-Path (Split-Path ([IO.Path]::GetFullPath($GeneratedDir)) -Parent) 'rexglue.cmake'
$text=[IO.File]::ReadAllText($cmake).Replace("`r`n","`n")
if($text.Contains('option(PDZ_USE_PRESERVED_GENERATED')){return}
$start=$text.IndexOf('add_custom_command(',$text.IndexOf('# Codegen runs as part of the build'))
$end=$text.IndexOf('# Include DLL module shared library targets',$start)
if($start -lt 0 -or $end -lt 0){throw 'Generated build codegen section not recognized'}
$block=$text.Substring($start,$end-$start)
if(!$block.Contains('TARGET_FILE:rex::rexglue') -or !$block.Contains('add_custom_target(perfectdarkzerorecomp_codegen')){throw 'Generated command contract changed'}
$replacement='option(PDZ_USE_PRESERVED_GENERATED "Use validated, repaired guest sources during migration" ON)'+"`nif(NOT PDZ_USE_PRESERVED_GENERATED)`n"+$block+"else()`nadd_custom_target(perfectdarkzerorecomp_codegen)`nendif()`n`n"
[IO.File]::WriteAllText($cmake,$text.Substring(0,$start)+$replacement+$text.Substring($end))
'PASS: default build preserves explicitly generated/repaired sources; automatic SDK generation requires explicit opt-in'
