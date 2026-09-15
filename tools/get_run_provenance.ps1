param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,

    [Parameter(Mandatory = $true)]
    [string]$ExpectedDiagnosticMode
)

$ErrorActionPreference = "Stop"
$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$CurrentRunPath = Join-Path $GameDirectory ".cojvr-run.json"
$StageStatePath = Join-Path $GameDirectory ".cojvr-d3d9-stage.json"
$LogPath = Join-Path $GameDirectory "cojvr.log"

foreach ($RequiredPath in @($CurrentRunPath, $StageStatePath, $LogPath)) {
    if (-not (Test-Path -LiteralPath $RequiredPath -PathType Leaf)) {
        throw "Required run-provenance file was not found: '$RequiredPath'."
    }
}

$Run = Get-Content -LiteralPath $CurrentRunPath -Raw | ConvertFrom-Json
$StageState = Get-Content -LiteralPath $StageStatePath -Raw | ConvertFrom-Json
$RunId = [string]$Run.runId
$BuildManifestId = [string]$Run.buildManifestId
if ([string]::IsNullOrWhiteSpace($RunId) -or $RunId -notmatch "^[A-Za-z0-9._-]+$") {
    throw "The current run has an invalid run ID."
}
if ([string]::IsNullOrWhiteSpace($BuildManifestId) -or $BuildManifestId -notmatch "^[A-Fa-f0-9]{64}$") {
    throw "The current run has an invalid build manifest ID."
}
if ([string]$Run.diagnosticMode -ne $ExpectedDiagnosticMode) {
    throw "Current run mode '$($Run.diagnosticMode)' does not match '$ExpectedDiagnosticMode'."
}
if ([string]$StageState.runId -ne $RunId -or
    [string]$StageState.buildManifestId -ne $BuildManifestId -or
    [string]$StageState.diagnosticMode -ne $ExpectedDiagnosticMode) {
    throw "The D3D9 staging state does not match the current run manifest."
}

$BuildManifestPath = Join-Path `
    (Join-Path (Join-Path (Join-Path $GameDirectory ".cojvr-evidence") "runs") $RunId) `
    "build-manifest.json"
if (-not (Test-Path -LiteralPath $BuildManifestPath -PathType Leaf)) {
    throw "The run-bound build manifest was not found at '$BuildManifestPath'."
}
$BuildManifestHash = (Get-FileHash -LiteralPath $BuildManifestPath -Algorithm SHA256).Hash.ToUpperInvariant()
if ($BuildManifestHash -ne ([string]$Run.buildManifestSha256).ToUpperInvariant() -or
    $BuildManifestHash -ne ([string]$StageState.buildManifestSha256).ToUpperInvariant()) {
    throw "The run-bound build manifest hash does not match staging metadata."
}
$BuildManifest = Get-Content -LiteralPath $BuildManifestPath -Raw | ConvertFrom-Json
if ([string]$BuildManifest.manifestId -ne $BuildManifestId) {
    throw "The run-bound build manifest ID does not match the current run."
}

foreach ($Deployment in @($Run.deployment)) {
    $Destination = Join-Path $GameDirectory ([string]$Deployment.destination)
    if (-not (Test-Path -LiteralPath $Destination -PathType Leaf)) {
        throw "Run-bound deployed file is missing: '$Destination'."
    }
    $ActualHash = (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($ActualHash -ne ([string]$Deployment.sha256).ToUpperInvariant()) {
        throw "Run-bound deployed file changed: '$Destination'."
    }
}

$Lines = Get-Content -LiteralPath $LogPath
$EscapedRunId = [Regex]::Escape($RunId)
$EscapedBuildManifestId = [Regex]::Escape($BuildManifestId)
if (-not [bool]($Lines -match "run_start: run_id=$EscapedRunId build_manifest_id=$EscapedBuildManifestId(?:\s|$)")) {
    throw "The log does not belong to the currently staged run/build manifest."
}

Write-Host "PASS - run ID bound to current staging state and log: $RunId"
Write-Host "PASS - build manifest ID and deployed hashes verified: $BuildManifestId"

[pscustomobject]@{
    RunId = $RunId
    BuildManifestId = $BuildManifestId
    Run = $Run
    StageState = $StageState
    BuildManifest = $BuildManifest
    Lines = $Lines
}
