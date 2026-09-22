param([switch]$Diagnostics)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$source = Join-Path $workspace 'bin\MelonRind'
if (-not (Test-Path -LiteralPath (Join-Path $source 'MelonRind.dll'))) { throw 'Build MelonRind first.' }
$manifest = Get-Content -LiteralPath (Join-Path $workspace 'manifest.json') -Raw
$manifest = $manifest.Replace('${modName}', 'MelonRind').Replace('${modFile}', 'MelonRind.dll').Replace('${modVersion}', '0.1.0-dev')
Set-Content -LiteralPath (Join-Path $source 'manifest.json') -Value $manifest -Encoding utf8
Copy-Item -LiteralPath (Join-Path $workspace 'resource_packs') -Destination $source -Recurse -Force
if (Test-Path -LiteralPath (Join-Path $workspace 'docs\testing.zh-CN.md')) {
    Copy-Item -LiteralPath (Join-Path $workspace 'docs\testing.zh-CN.md') -Destination (Join-Path $source 'TESTING.md') -Force
}
$configDir = Join-Path $source 'config'
New-Item -ItemType Directory -Path $configDir -Force | Out-Null
# Always write staging defaults: a normal package must not inherit diagnostic
# markers from a preceding -Diagnostics invocation. Installed settings are separate.
$config = [ordered]@{ diagnostics=[bool]$Diagnostics; diagnosticMarkers=[bool]$Diagnostics; positionsAreLocal=$true; renderPass=0; saturationCap='hungerAfterEating' }
$config | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $configDir 'config.json') -Encoding utf8
$dist = Join-Path $workspace 'dist'
New-Item -ItemType Directory -Path $dist -Force | Out-Null
$archive = Join-Path $dist 'MelonRind-0.1.0-dev-client.zip'
Compress-Archive -Path $source -DestinationPath $archive -Force
Get-FileHash -LiteralPath $archive -Algorithm SHA256
