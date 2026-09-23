param(
    [switch]$Diagnostics,
    [string]$Version = '0.1.1',
    [string]$ArchiveName = ''
)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$source = Join-Path $workspace 'bin\MelonRind'
if (-not (Test-Path -LiteralPath (Join-Path $source 'MelonRind.dll'))) { throw 'Build MelonRind first.' }

if (-not $ArchiveName) {
    $ArchiveName = "MelonRind-$Version-client-windows-x64.zip"
}

# Update manifest.json in source
$manifest = Get-Content -LiteralPath (Join-Path $workspace 'manifest.json') -Raw
$manifest = $manifest.Replace('${modName}', 'MelonRind').Replace('${modFile}', 'MelonRind.dll').Replace('${modVersion}', $Version)
Set-Content -LiteralPath (Join-Path $source 'manifest.json') -Value $manifest -Encoding utf8

Copy-Item -LiteralPath (Join-Path $workspace 'resource_packs') -Destination $source -Recurse -Force
if (Test-Path -LiteralPath (Join-Path $workspace 'LICENSE')) {
    Copy-Item -LiteralPath (Join-Path $workspace 'LICENSE') -Destination (Join-Path $source 'LICENSE') -Force
}
if (Test-Path -LiteralPath (Join-Path $workspace 'docs\testing.zh-CN.md')) {
    Copy-Item -LiteralPath (Join-Path $workspace 'docs\testing.zh-CN.md') -Destination (Join-Path $source 'TESTING.md') -Force
}

$configDir = Join-Path $source 'config'
New-Item -ItemType Directory -Path $configDir -Force | Out-Null
$config = [ordered]@{ diagnostics=[bool]$Diagnostics; diagnosticMarkers=[bool]$Diagnostics; positionsAreLocal=$true; renderPass=0; saturationCap='hungerAfterEating' }
$config | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $configDir 'config.json') -Encoding utf8

# Stage package content: MelonRind directory + root tooth.json
$dist = Join-Path $workspace 'dist'
New-Item -ItemType Directory -Path $dist -Force | Out-Null
$staging = Join-Path $dist 'staging'
if (Test-Path -LiteralPath $staging) {
    Remove-Item -LiteralPath $staging -Recurse -Force
}
New-Item -ItemType Directory -Path $staging -Force | Out-Null

# Copy MelonRind directory
Copy-Item -LiteralPath $source -Destination (Join-Path $staging 'MelonRind') -Recurse -Force

$archive = Join-Path $dist $ArchiveName
Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $archive -Force
Remove-Item -LiteralPath $staging -Recurse -Force

Get-FileHash -LiteralPath $archive -Algorithm SHA256

