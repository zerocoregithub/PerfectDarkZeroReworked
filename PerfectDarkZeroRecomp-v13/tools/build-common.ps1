param(
    [ValidateSet('dev','profile','release')][string]$Mode = 'release',
    [ValidateRange(1,32)][int]$Jobs = 4,
    [string]$SDKSource = (Join-Path $PSScriptRoot '../../rexglue-sdk-v13'),
    [string]$BuildDirectory = '',
    [string]$PythonExecutable = '',
    [string]$Recompiler = '',
    [switch]$DeveloperDiagnostics,
    [switch]$PlanOnly
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$sdk = (Resolve-Path -LiteralPath $SDKSource).Path
if (!(Test-Path -LiteralPath (Join-Path $sdk 'include/rex/ui/hdr_output.h'))) {
    throw 'Select the matched modified SDK; an unmodified upstream SDK is insufficient.'
}
if (!$BuildDirectory) { $BuildDirectory = Join-Path $repo "out/$Mode" }
$output = [IO.Path]::GetFullPath($BuildDirectory)
$preset = if ($Mode -eq 'dev') {'win-amd64-relwithdebinfo'} else {'win-amd64-release'}
$diagnostics = if ($DeveloperDiagnostics) {'ON'} else {'OFF'}
$quietLogging = if ($DeveloperDiagnostics) {'OFF'} else {'ON'}
$sourceArchive = if (Test-Path -LiteralPath (Join-Path $sdk 'SOURCE_ARCHIVE.json')) {'ON'} else {'OFF'}
# Keep developer usernames/source locations out of __FILE__ strings and debug
# paths. Map only this game's source/SDK/output trees, not arbitrary user paths.
$portablePaths = "-ffile-prefix-map=`"$repo=pdz-game`" -ffile-prefix-map=`"$sdk=rexglue-sdk`" -ffile-prefix-map=`"$output=pdz-build`""
$configure = @('--preset',$preset,'-B',$output,"-DREXSDK_DIR=$sdk",
    "-DCMAKE_C_FLAGS=-mssse3 $portablePaths","-DCMAKE_CXX_FLAGS=-mssse3 $portablePaths",
    "-DREX_DISABLE_RUNTIME_LOGGING=$quietLogging",
    "-DREXGLUE_SOURCE_ARCHIVE=$sourceArchive",
    "-DPDZ_ENABLE_DEVELOPER_DIAGNOSTICS=$diagnostics")
if ($Mode -eq 'dev') {
    $configure += @('-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO=Release')
}
if ($Mode -eq 'profile') {
    $configure += @('-DCMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG -gcodeview',
        '-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -gcodeview',
        '-DCMAKE_EXE_LINKER_FLAGS_RELEASE=-Xlinker /DEBUG:FULL',
        '-DCMAKE_SHARED_LINKER_FLAGS_RELEASE=-Xlinker /DEBUG:FULL')
}
if ($PythonExecutable) {
    $python = (Resolve-Path -LiteralPath $PythonExecutable).Path
    $configure += @("-DPYTHON_EXECUTABLE=$python","-DPython3_EXECUTABLE=$python")
}
if ($PlanOnly) {
    [pscustomobject]@{Mode=$Mode;SDK=$sdk;Output=$output;Configure=$configure;
        Diagnostics=$diagnostics;QuietLogging=$quietLogging;SourceArchive=$sourceArchive;Deploys=$false;Recompiler=$Recompiler}
    return
}
if (!(Test-Path -LiteralPath (Join-Path $repo 'generated/default/sources.cmake'))) {
    if (!$Recompiler) { throw 'First generation requires -Recompiler with the matched SDK generator. Use an isolated checkout while regeneration drift is under review.' }
    $Recompiler = (Resolve-Path -LiteralPath $Recompiler).Path
    $partitionSeed=Join-Path $repo 'codegen/codegen.partition.json'
    if(!(Test-Path -LiteralPath $partitionSeed)){throw 'Missing versioned partition seed; do not generate a new anonymous layout.'}
}
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ Build Tools are missing.' }
& "$vs/Common7/Tools/Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
Push-Location $repo
$previousPath = $env:PATH
try {
    $env:PATH = "$vs/VC/Tools/Llvm/x64/bin;$vs/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin;$vs/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja;$env:PATH"
    if (!(Test-Path -LiteralPath 'generated/default/sources.cmake')) {
        $input=Join-Path $repo 'assets/default.xex'
        if(!(Test-Path -LiteralPath $input) -or (Get-FileHash -LiteralPath $input).Hash -ne 'E50192F437BD1FF70A65FAE8B5561AA822AB91052FE35FD500594689E130859E'){throw 'Generation requires the supported legally supplied US/NTSC-U default.xex; input missing or unsupported revision.'}
        New-Item -ItemType Directory -Force 'generated/default'|Out-Null
        Copy-Item -LiteralPath $partitionSeed -Destination 'generated/default/codegen.partition.json'
        & $Recompiler codegen (Join-Path $repo 'perfectdarkzerorecomp_manifest.toml')
        if ($LASTEXITCODE -ne 0) { throw 'Code generation failed.' }
    }
    # The same adapter runs manually here and through CMake after regeneration.
    & (Join-Path $PSScriptRoot 'fix-unresolved-tail-jumps.ps1') -GeneratedDir (Join-Path $repo 'generated/default')
    & (Join-Path $PSScriptRoot 'test-codegen-contract.ps1') -GeneratedDir (Join-Path $repo 'generated/default')
    & (Join-Path $PSScriptRoot 'patch-generated-build.ps1') -GeneratedDir (Join-Path $repo 'generated/default')
    & cmake @configure
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & cmake --build $output --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
} finally {
    $env:PATH = $previousPath
    Pop-Location
}
