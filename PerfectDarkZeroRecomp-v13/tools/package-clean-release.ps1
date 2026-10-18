param([Parameter(Mandatory)][string]$Binaries,
      [Parameter(Mandatory)][string]$Launcher,
      [Parameter(Mandatory)][string]$CRTDirectory,
      [Parameter(Mandatory)][string]$Destination,
      [string]$SDKSource=(Join-Path $PSScriptRoot '../../rexglue-sdk-v13'),
      [string]$Version='v0.9.0-rc1')
$ErrorActionPreference='Stop'
$game=Split-Path $PSScriptRoot -Parent
$target=[IO.Path]::GetFullPath($Destination)
if($Version -notmatch '^v[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$'){throw 'Invalid version label'}
if(Test-Path -LiteralPath $target){throw 'New portable release folder required'}
$copies=[Collections.Generic.List[object]]::new()
function AddFile([string]$source,[string]$relative) {
    if([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|/)\.\.(/|$)'){throw 'Unsafe payload path'}
    $destination=Join-Path $target $relative
    New-Item -ItemType Directory -Force (Split-Path $destination)|Out-Null
    Copy-Item -LiteralPath $source -Destination $destination
    $hash=(Get-FileHash -LiteralPath $source).Hash
    if((Get-FileHash -LiteralPath $destination).Hash -ne $hash){throw 'Payload copy mismatch'}
    $copies.Add([pscustomobject]@{File=$relative;SHA256=$hash})
}
New-Item -ItemType Directory $target,"$target/ROM"|Out-Null
AddFile $Launcher "PerfectDarkZeroReworked-$Version.exe"
foreach($file in @('perfectdarkzerorecomp.exe','rexruntime.dll','rexgpu-xenos.dll')){AddFile (Join-Path $Binaries $file) "Data/$file"}
foreach($file in @('concrt140.dll','msvcp140.dll','msvcp140_1.dll','msvcp140_2.dll','msvcp140_atomic_wait.dll','msvcp140_codecvt_ids.dll','vccorlib140.dll','vcruntime140.dll','vcruntime140_1.dll','vcruntime140_threads.dll')){AddFile (Join-Path $CRTDirectory $file) "Data/$file"}
foreach($file in @('hardware.toml','mapping.toml')){AddFile "$game/release/defaults/$file" "Data/settings/$file"}
foreach($file in @('README.md','KNOWN_ISSUES.md','BUILDING.md','THIRD_PARTY_NOTICES.md')){AddFile "$game/release/$file" $file}
AddFile "$game/LICENSE" 'Data/Licenses/Project-MIT.txt'
AddFile "$SDKSource/LICENSE" 'Data/Licenses/ReXGlue-BSD.txt'
foreach($entry in @(Get-Content "$game/release/NOTICES-MANIFEST.json" -Raw | ConvertFrom-Json)) {
    $relative=$entry.File
    if([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|[/\\])\.\.([/\\]|$)|:'){throw 'Unsafe notice path'}
    $source=Join-Path "$game/release/licenses" $relative
    if((Get-FileHash -LiteralPath $source).Hash -ne $entry.SHA256){throw 'Reviewed notice changed'}
    AddFile $source "Data/Licenses/thirdparty/$relative"
}
if(@(Get-ChildItem -LiteralPath $target -Recurse -File).Count -ne $copies.Count){throw 'Unexpected portable payload file'}
$manifestPath=$target+'.manifest.csv'
$copies|Export-Csv -LiteralPath $manifestPath -NoTypeInformation
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($target,$target+'.zip')
$zip=[IO.Compression.ZipFile]::OpenRead($target+'.zip')
try {
    $entries=@($zip.Entries|Where-Object {$_.Name})
    if($entries.Count -ne $copies.Count){throw 'ZIP count mismatch'}
    foreach($entry in $entries){
        $record=$copies|Where-Object File -EQ $entry.FullName
        if(!$record){throw 'Non-allowlisted ZIP entry'}
        $stream=$entry.Open()
        try{$hash=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($stream))}finally{$stream.Dispose()}
        if($hash -ne $record.SHA256){throw 'ZIP hash mismatch'}
    }
} finally {$zip.Dispose()}
"PASS: $($copies.Count) payload files/ZIP hashes; authored defaults only, no game assets/saves/caches/logs/symbols; NOT gameplay acceptance or upload approval"
