param([Parameter(Mandatory)][string]$WorkingDirectory,
      [string]$SDKSource=(Join-Path $PSScriptRoot '../../rexglue-sdk-v13'))
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$work=[IO.Path]::GetFullPath($WorkingDirectory)
if(Test-Path -LiteralPath $work){throw 'Use a new private test directory'}
New-Item -ItemType Directory -Path $work | Out-Null
foreach($name in @('build-common.ps1','build-dev.ps1','build-profile.ps1','build-release.ps1',
                   'fix-unresolved-tail-jumps.ps1','patch-generated-runtime.ps1','patch-vision-color.ps1')) {
    $parseErrors=$null;$tokens=$null
    [void][Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $name),[ref]$tokens,[ref]$parseErrors)
    if($parseErrors.Count){throw "PowerShell parse error: $name"}
}
foreach($mode in @('dev','profile','release')) {
    $plan=& (Join-Path $PSScriptRoot "build-$mode.ps1") -SDKSource $SDKSource -PlanOnly
    if($plan.Diagnostics -ne 'OFF' -or $plan.Deploys -or $plan.Mode -ne $mode){throw 'Unsafe/inconsistent build plan'}
    if($plan.SDK -ne (Resolve-Path -LiteralPath $SDKSource).Path){throw 'SDK selection lost'}
    if($plan.QuietLogging -ne 'ON' -or '-DREX_DISABLE_RUNTIME_LOGGING=ON' -notin $plan.Configure){throw 'Normal build must explicitly select sink-free logging'}
    if($plan.SourceArchive -ne 'OFF' -or '-DREXGLUE_SOURCE_ARCHIVE=OFF' -notin $plan.Configure){throw 'Normal Git SDK must not silently bypass dependency metadata checks'}
    if(!($plan.Configure -match 'ffile-prefix-map=')){throw 'Release path privacy mapping missing'}
}
$debugPlan=& (Join-Path $PSScriptRoot 'build-release.ps1') -SDKSource $SDKSource -PlanOnly -DeveloperDiagnostics
if($debugPlan.Diagnostics -ne 'ON'){throw 'Explicit diagnostic build unavailable'}
if($debugPlan.QuietLogging -ne 'OFF' -or '-DREX_DISABLE_RUNTIME_LOGGING=OFF' -notin $debugPlan.Configure){throw 'Explicit diagnostic build must restore logging'}
$cmake=[IO.File]::ReadAllText((Join-Path $repo 'CMakeLists.txt'))
foreach($helper in @('patch-generated-runtime.ps1','apply-p9p-visibility-hook.ps1','patch-vision-color.ps1')) {
    if(!$cmake.Contains('"${CMAKE_CURRENT_SOURCE_DIR}/tools/'+$helper+'"')){throw "Missing helper dependency: $helper"}
}
$live=Join-Path $repo 'generated/default'
$before=@{}; Get-ChildItem $live -File | ForEach-Object {$before[$_.Name]=(Get-FileHash -LiteralPath $_.FullName).Hash}
$target=Join-Path $work 'generated'
Copy-Item -LiteralPath $live -Destination $target -Recurse
# Exercise migration from the old accepted hook shape, not only already-clean
# sources. These edits touch the private fixture directory only.
$routePath=Join-Path $target 'perfectdarkzerorecomp_recomp.10.cpp'
$routeText=[IO.File]::ReadAllText($routePath)
$routeText=[regex]::Replace($routeText,'(?m)^\s*PDZ_ObserveDodgeRoll\([^\r\n]*\);\r?\n','')
$routeText=[regex]::Replace($routeText,'(?m)^\s*PDZ_TraceDodgeRoute\([^\r\n]*\);\r?\n','')
[IO.File]::WriteAllText($routePath,$routeText)
$visionPath=Join-Path $target 'perfectdarkzerorecomp_recomp.99.cpp'
$visionText=[IO.File]::ReadAllText($visionPath)
$visionText='extern "C" unsigned int PDZ_VisionColorMode(unsigned int);'+"`n"+$visionText
$visionText=$visionText.Replace('ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 732);',
    'ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 732);'+"`n`tctx.r11.u64 = PDZ_VisionColorMode(ctx.r11.u32);")
[IO.File]::WriteAllText($visionPath,$visionText)
& (Join-Path $PSScriptRoot 'fix-unresolved-tail-jumps.ps1') -GeneratedDir $target
$first=@{}; Get-ChildItem $target -File | ForEach-Object {$first[$_.Name]=(Get-FileHash -LiteralPath $_.FullName).Hash}
& (Join-Path $PSScriptRoot 'fix-unresolved-tail-jumps.ps1') -GeneratedDir $target
foreach($file in Get-ChildItem $target -File) {
    if($first[$file.Name] -ne (Get-FileHash -LiteralPath $file.FullName).Hash){throw "Adapter not idempotent: $($file.Name)"}
}
foreach($file in Get-ChildItem $live -File) {
    if($before[$file.Name] -ne (Get-FileHash -LiteralPath $file.FullName).Hash){throw "Live generated file modified: $($file.Name)"}
}
$aim=[IO.File]::ReadAllText((Join-Path $target 'perfectdarkzerorecomp_recomp.112.cpp'))
foreach($hook in @('PDZ_ConstrainThirdPersonTarget\(','PDZ_TraceCoverAimOrder\(base, 0, 3,')) {
    if([regex]::Matches($aim,$hook).Count -ne 1){throw "Target hook missing/duplicated: $hook"}
}
$route=[IO.File]::ReadAllText((Join-Path $target 'perfectdarkzerorecomp_recomp.10.cpp'))
if(([regex]::Matches($route,'PDZ_ObserveDodgeRoll\(')).Count -ne 1){throw 'Fatigue observer missing or duplicated'}
$start=$route.IndexOf('DEFINE_REX_FUNC(sub_82685188) {')
$end=$route.IndexOf('DEFINE_REX_FUNC(', $start+1)
$body=$route.Substring($start,$end-$start)
if($body.IndexOf('PDZ_ObserveDodgeRoll(') -lt 0 -or
   $body.IndexOf('PDZ_ObserveDodgeRoll(') -gt $body.IndexOf('PDZ_TraceDodgeRoute(')){throw 'Gameplay observer order changed'}
$vision=[IO.File]::ReadAllText((Join-Path $target 'perfectdarkzerorecomp_recomp.99.cpp'))
if($vision.Contains('PDZ_VisionColorMode')){throw 'Retired identity hook remains'}
$qol=[IO.File]::ReadAllText((Join-Path $repo 'src/pdz_qol.cpp'))
if($qol.Contains('PDZ_P9PVisibilityScope::')){throw 'Retired P9P override remains'}
'PASS: script parsing, shared build plans, diagnostics default OFF, adapter isolation/idempotence, gameplay observer order, retired hooks removed.'
