param([ValidateRange(1,32)][int]$Jobs=4,
      [string]$SDKSource=(Join-Path $PSScriptRoot '../../rexglue-sdk-v13'),
      [string]$BuildDirectory='', [string]$PythonExecutable='', [string]$Recompiler='',
      [switch]$DeveloperDiagnostics, [switch]$PlanOnly)
& (Join-Path $PSScriptRoot 'build-common.ps1') -Mode release @PSBoundParameters
