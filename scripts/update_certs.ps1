param(
    [string] $PemUrl = "https://curl.se/ca/cacert.pem",
    [string] $InputPem = "data/cert/cacrt_all.pem",
    [string] $OutputBin = "data/cert/x509_crt_bundle.bin",
    [switch] $SkipDownload
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$inputPemPath = Join-Path $repoRoot $InputPem
$outputBinPath = Join-Path $repoRoot $OutputBin
$generatorPath = Join-Path $PSScriptRoot "gen_crt_bundle.ps1"

if (-not (Test-Path $generatorPath)) {
    throw "Missing certificate bundle generator: $generatorPath"
}

New-Item -ItemType Directory -Force -Path (Split-Path $inputPemPath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path $outputBinPath) | Out-Null

if (-not $SkipDownload) {
    Write-Host "Downloading CA PEM bundle from $PemUrl"
    Invoke-WebRequest -Uri $PemUrl -OutFile $inputPemPath
} elseif (-not (Test-Path $inputPemPath)) {
    throw "Input PEM does not exist: $inputPemPath"
}

Write-Host "Generating ESP x509 certificate bundle"
& $generatorPath -InputPem $inputPemPath -OutputBin $outputBinPath

Write-Host "Updated:"
Write-Host "  $inputPemPath"
Write-Host "  $outputBinPath"
