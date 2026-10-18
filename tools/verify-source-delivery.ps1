param([string]$Root=(Join-Path $PSScriptRoot '..'))
$ErrorActionPreference='Stop'
$Root=(Resolve-Path -LiteralPath $Root).Path
$game=Join-Path $Root 'PerfectDarkZeroRecomp-v13'
$sdk=Join-Path $Root 'rexglue-sdk-v13'
$count=0
$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
function Assert-SourceHash([string]$Base,[string]$Name,[string]$Expected){
 if(!$Name -or $Name.Contains('\') -or $Name.Contains(':') -or
    [IO.Path]::IsPathRooted($Name) -or
    @($Name.Split('/') | Where-Object {$_ -in @('','.','..')}).Count -or
    $Expected -notmatch '^[0-9a-fA-F]{64}$'){
  throw "Unsafe manifest entry: $Name"
 }
 if(!$seen.Add("$Base/$Name")){throw "Duplicate manifest entry: $Name"}
 $file=Join-Path $Base $Name
 if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $Expected){throw "Source mismatch: $Name"}
}
foreach($record in (Import-Csv -LiteralPath (Join-Path $game 'SOURCE-MANIFEST.csv'))){
 Assert-SourceHash $game $record.File $record.SHA256
 $count++
}
$archive=Get-Content -LiteralPath (Join-Path $sdk 'SOURCE_ARCHIVE.json') -Raw | ConvertFrom-Json
if($archive.format -ne 1 -or !$archive.files){throw 'Unsupported SDK source manifest'}
foreach($record in $archive.files){
 Assert-SourceHash $sdk $record.path $record.sha256
 $count++
}
"PASS: $count game/SDK source hashes verified; no compilation or game launch"
