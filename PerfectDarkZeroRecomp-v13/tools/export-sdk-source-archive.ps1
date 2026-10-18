param([Parameter(Mandatory)][string]$SDKSource,
      [Parameter(Mandatory)][string]$Destination)
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath $SDKSource).Path
$target=[IO.Path]::GetFullPath($Destination)
if(!(Test-Path "$source/include/rex/runtime.h") -or !(Test-Path "$source/src/core/logging.cpp")){throw 'Expected the exact SDK source root, not a workspace or game installation'}
if($target.Equals($source,[StringComparison]::OrdinalIgnoreCase) -or $target.StartsWith($source+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Archive destination must be outside SDK source'}
if(Test-Path -LiteralPath $target){throw 'Source archive destination must be new'}
New-Item -ItemType Directory $target|Out-Null
$records=[Collections.Generic.List[object]]::new()
foreach($file in Get-ChildItem -LiteralPath $source -File -Recurse|Sort-Object FullName) {
    $relative=[IO.Path]::GetRelativePath($source,$file.FullName).Replace('\','/')
    if($relative -match '(^|/)(\.git|\.aws|\.ssh|\.codex|\.agents|__pycache__|out|build|cache|logs|captures|profiles|user-data|\.vs)(/|$)' -or
       $file.Name -match '^\.env($|\.)|^id_(rsa|ed25519)$' -or
       $file.Name -in @('SOURCE_ARCHIVE.json','CMakeUserPresets.json') -or
       $file.Extension -in @('.exe','.dll','.obj','.pdb','.dmp','.iso','.xex','.pkg','.rdc','.log','.bin','.cso','.spv','.pyc','.pyo','.o','.a','.so','.dylib','.res','.ilk','.exp','.idb','.ipdb','.iobj','.tlog','.cache') -or
       ($file.Extension -eq '.lib' -and $file.Name -ne 'COPYING.LIB')){continue}
    if($file.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Review source link before export: $relative"}
    $destination=Join-Path $target $relative
    New-Item -ItemType Directory -Force (Split-Path $destination)|Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $destination
    $hash=(Get-FileHash -LiteralPath $file.FullName).Hash
    if((Get-FileHash -LiteralPath $destination).Hash -ne $hash){throw 'Source copy hash mismatch'}
    $records.Add([pscustomobject]@{path=$relative;sha256=$hash})
}
@{format=1;description='Modified SDK source snapshot; no guest game data or Git history; integrity manifest, not licence certification';files=@($records)}|
    ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $target 'SOURCE_ARCHIVE.json') -Encoding utf8NoBOM
"Prepared $($records.Count) hashed SDK source files for review; verify and configure before redistribution"
