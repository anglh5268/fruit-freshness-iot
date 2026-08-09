param(
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$distDirectory = Join-Path $projectRoot "dist"

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $distDirectory "spectrum_gateway_vefaas.zip"
}

New-Item -ItemType Directory -Force -Path $distDirectory | Out-Null
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)
$archiveEntries = @(
    "cloud_gateway/__init__.py"
    "cloud_gateway/ark_client.py"
    "cloud_gateway/engine.py"
    "cloud_gateway/mock_engine.py"
    "cloud_gateway/prompt_builder.py"
    "cloud_gateway/schemas.py"
    "cloud_gateway/server.py"
    "cloud_gateway/settings.py"
)

foreach ($entry in $archiveEntries) {
    $sourcePath = Join-Path $projectRoot ($entry.Replace("/", [System.IO.Path]::DirectorySeparatorChar))
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Required deployment file is missing: $entry"
    }
}

if (Test-Path -LiteralPath $OutputPath) {
    Remove-Item -LiteralPath $OutputPath -Force
}

$tar = Get-Command "tar.exe" -ErrorAction Stop
Push-Location $projectRoot
try {
    & $tar.Source -a -c -f $OutputPath @archiveEntries
    if ($LASTEXITCODE -ne 0) {
        throw "tar.exe failed to create the deployment ZIP (exit code $LASTEXITCODE)."
    }
} finally {
    Pop-Location
}

$archiveListing = @(& $tar.Source -tf $OutputPath)
if ($LASTEXITCODE -ne 0) {
    throw "tar.exe failed to inspect the deployment ZIP (exit code $LASTEXITCODE)."
}

$invalidEntries = @($archiveListing | Where-Object { $_ -match "\\" })
if ($invalidEntries.Count -gt 0) {
    throw "ZIP contains Windows backslash paths: $($invalidEntries -join ', ')"
}
foreach ($expectedEntry in $archiveEntries) {
    if ($archiveListing -notcontains $expectedEntry) {
        throw "ZIP is missing expected entry: $expectedEntry"
    }
}

Write-Host "veFaaS deployment package created:" -ForegroundColor Green
Write-Host $OutputPath
Write-Host "Verified archive entries (Linux-compatible forward slashes):"
$archiveListing | ForEach-Object { Write-Host "  $_" }
Write-Host "The package contains Python source only; .env and API keys are excluded."
