param([Parameter(Mandatory)][string]$ReviewedManifest,
      [Parameter(Mandatory)][string]$Destination,
      [string]$SourceRoot=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $SourceRoot).Path
$destinationPath=[IO.Path]::GetFullPath($Destination)
if(Test-Path -LiteralPath $destinationPath){throw 'Public source staging must be new'}
$records=@(Get-Content -LiteralPath $ReviewedManifest -Raw|ConvertFrom-Json)
if(!$records.Count){throw 'Empty source allowlist'}
$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($entry in $records) {
    foreach($name in @($entry.Source,$entry.Target)) {
        if([IO.Path]::IsPathRooted($name) -or $name.Contains('\') -or $name -match '(^|/)\.{1,2}(/|$)|:|//'){throw 'Unsafe source export path'}
        if($name -match '(^|/)(assets|user-data|profiles|research|out|cache|captures|settings|\.git)(/|$)' -or $name -match '^generated/default/'){throw 'Private/game/generated material is not a public source input'}
        if([IO.Path]::GetExtension($name) -in @('.iso','.xex','.pkg','.rdc','.exe','.dll','.pdb','.obj','.pdnf','.ttf','.bin','.ico','.png','.jpg','.jpeg','.dds')){throw 'Binary/game asset or unreviewed artwork is not a public source input'}
    }
    if(!$seen.Add($entry.Target)){throw 'Duplicate export target'}
    $source=Join-Path $root $entry.Source
    if((Get-FileHash -LiteralPath $source).Hash -ne $entry.SHA256){throw "Reviewed source changed: $($entry.Source)"}
}
New-Item -ItemType Directory $destinationPath|Out-Null
$copied=foreach($entry in $records) {
    $target=Join-Path $destinationPath $entry.Target
    New-Item -ItemType Directory -Force (Split-Path $target)|Out-Null
    Copy-Item -LiteralPath (Join-Path $root $entry.Source) -Destination $target
    if((Get-FileHash -LiteralPath $target).Hash -ne $entry.SHA256){throw 'Export copy mismatch'}
    [pscustomobject]@{File=$entry.Target;SHA256=$entry.SHA256}
}
$copied|Export-Csv (Join-Path $destinationPath 'SOURCE-MANIFEST.csv') -NoTypeInformation
"PASS: $($copied.Count) explicitly allowlisted source files; no Git history or recursive workspace copy"
