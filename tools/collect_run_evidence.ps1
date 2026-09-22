param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,

    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

function Get-UpperSha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Copy-EvidenceFile(
    [string]$Source,
    [string]$Destination,
    [System.Collections.Generic.List[object]]$Inventory) {
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        return
    }
    $DestinationDirectory = [System.IO.Path]::GetDirectoryName($Destination)
    if (-not (Test-Path -LiteralPath $DestinationDirectory -PathType Container)) {
        New-Item -ItemType Directory -Path $DestinationDirectory -Force | Out-Null
    }
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
    $Inventory.Add([ordered]@{
        path = $Destination.Substring($RunDirectory.Length).TrimStart("\").Replace("\", "/")
        sha256 = Get-UpperSha256 $Destination
        size = (Get-Item -LiteralPath $Destination).Length
    })
}

$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
if (Get-Process -Name CoJ -ErrorAction SilentlyContinue) {
    throw "Call of Juarez is running. Close it before collecting final run evidence."
}

$CurrentRunPath = Join-Path $GameDirectory ".cojvr-run.json"
if (-not (Test-Path -LiteralPath $CurrentRunPath -PathType Leaf)) {
    throw "No .cojvr-run.json was found. Stage an identified candidate before collecting evidence."
}
$CurrentRun = Get-Content -LiteralPath $CurrentRunPath -Raw | ConvertFrom-Json
$RunId = [string]$CurrentRun.runId
if ([string]::IsNullOrWhiteSpace($RunId) -or $RunId -notmatch "^[A-Za-z0-9._-]+$") {
    throw "The current run manifest has an invalid runId."
}

$EvidenceRoot = Join-Path $GameDirectory ".cojvr-evidence"
$RunDirectory = Join-Path (Join-Path $EvidenceRoot "runs") $RunId
if (-not (Test-Path -LiteralPath $RunDirectory -PathType Container)) {
    throw "Run evidence directory '$RunDirectory' is missing. Staging did not complete provenance setup."
}

$Inventory = [System.Collections.Generic.List[object]]::new()
Copy-EvidenceFile $CurrentRunPath (Join-Path $RunDirectory "run-manifest.json") $Inventory

$StageStatePaths = @(
    (Join-Path $GameDirectory ".cojvr-d3d9-stage.json"),
    (Join-Path $GameDirectory ".cojvr-openvr-flat-stage.json")
)
foreach ($StageStatePath in $StageStatePaths) {
    if (Test-Path -LiteralPath $StageStatePath -PathType Leaf) {
        $StageState = Get-Content -LiteralPath $StageStatePath -Raw | ConvertFrom-Json
        if ([string]$StageState.runId -ne $RunId) {
            throw "Staging state '$StageStatePath' belongs to a different run."
        }
        Copy-EvidenceFile `
            $StageStatePath `
            (Join-Path $RunDirectory "deployment\$([System.IO.Path]::GetFileName($StageStatePath))") `
            $Inventory
    }
}

$RuntimeLog = Join-Path $GameDirectory "cojvr.log"
$Callstack = Join-Path $GameDirectory "callstack.txt"
$CameraControl = Join-Path $GameDirectory "cojvr-camera-control.json"
$NativeStereoSummary = Join-Path $GameDirectory "cojvr-native-stereo-summary.json"
$MovementSummary = Join-Path $GameDirectory "cojvr-movement-summary.json"
$RequirePerformanceSummary = [bool]$CurrentRun.validation.requirePerformanceSummary
if ($RequirePerformanceSummary -and
    -not (Test-Path -LiteralPath $NativeStereoSummary -PathType Leaf)) {
    throw "This run requires a native-stereo performance/state summary before evidence collection."
}
Copy-EvidenceFile $RuntimeLog (Join-Path $RunDirectory "runtime\cojvr.log") $Inventory
Copy-EvidenceFile $Callstack (Join-Path $RunDirectory "runtime\callstack.txt") $Inventory
Copy-EvidenceFile $CameraControl (Join-Path $RunDirectory "runtime\cojvr-camera-control.json") $Inventory
Copy-EvidenceFile $NativeStereoSummary (Join-Path $RunDirectory "analysis\native-stereo-summary.json") $Inventory
Copy-EvidenceFile $MovementSummary (Join-Path $RunDirectory "analysis\movement-summary.json") $Inventory

$RuntimeStarted = $false
$RuntimeEnded = $false
if (Test-Path -LiteralPath $RuntimeLog -PathType Leaf) {
    $LogLines = Get-Content -LiteralPath $RuntimeLog
    $EscapedRunId = [Regex]::Escape($RunId)
    $RuntimeStarted = [bool]($LogLines -match "run_start: run_id=$EscapedRunId(?:\s|$)")
    $RuntimeEnded = [bool]($LogLines -match "run_end: run_id=$EscapedRunId(?:\s|$)")
    foreach ($Line in $LogLines) {
        if (-not $Line.StartsWith("COJVR_EVENT ", [StringComparison]::Ordinal)) { continue }
        try {
            $Event = $Line.Substring(12) | ConvertFrom-Json
            if ([string]$Event.run_id -eq $RunId -and [string]$Event.event -eq "run_start") {
                $RuntimeStarted = $true
            }
            if ([string]$Event.run_id -eq $RunId -and [string]$Event.event -eq "run_end") {
                $RuntimeEnded = $true
            }
        } catch {
            # The structured verifier reports malformed lines. Collection still
            # preserves the raw log and marks the run incomplete if run_end is lost.
        }
    }
}

$DeploymentChecks = [System.Collections.Generic.List[object]]::new()
foreach ($Deployment in @($CurrentRun.deployment)) {
    $Destination = Join-Path $GameDirectory ([string]$Deployment.destination)
    $Exists = Test-Path -LiteralPath $Destination -PathType Leaf
    $ActualHash = if ($Exists) { Get-UpperSha256 $Destination } else { $null }
    $ExpectedHash = ([string]$Deployment.sha256).ToUpperInvariant()
    $DeploymentChecks.Add([ordered]@{
        destination = [string]$Deployment.destination
        expectedSha256 = $ExpectedHash
        actualSha256 = $ActualHash
        matches = $Exists -and $ActualHash -eq $ExpectedHash
    })
}

$EvidenceManifestPath = Join-Path $RunDirectory "evidence-manifest.json"
$EvidenceManifest = [ordered]@{
    schemaVersion = 1
    manifestType = "cojvr-run-evidence"
    runId = $RunId
    buildManifestId = [string]$CurrentRun.buildManifestId
    collectedUtc = [DateTime]::UtcNow.ToString("o")
    runtimeStarted = $RuntimeStarted
    runtimeEnded = $RuntimeEnded
    incomplete = -not $RuntimeEnded
    analysis = [ordered]@{
        nativeStereoSummaryRequired = $RequirePerformanceSummary
        nativeStereoSummaryCollected = Test-Path -LiteralPath $NativeStereoSummary -PathType Leaf
        movementSummaryCollected = Test-Path -LiteralPath $MovementSummary -PathType Leaf
    }
    deploymentChecks = @($DeploymentChecks)
    files = @($Inventory)
}
[System.IO.File]::WriteAllText(
    $EvidenceManifestPath,
    ($EvidenceManifest | ConvertTo-Json -Depth 10) + [Environment]::NewLine,
    [System.Text.UTF8Encoding]::new($false))

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $PackageDirectory = Join-Path $EvidenceRoot "packages"
    if (-not (Test-Path -LiteralPath $PackageDirectory -PathType Container)) {
        New-Item -ItemType Directory -Path $PackageDirectory -Force | Out-Null
    }
    $OutputPath = Join-Path $PackageDirectory "$RunId.zip"
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)
if (Test-Path -LiteralPath $OutputPath) {
    Remove-Item -LiteralPath $OutputPath -Force
}
Compress-Archive -Path (Join-Path $RunDirectory "*") -DestinationPath $OutputPath -CompressionLevel Optimal

Write-Host "Run evidence package: $OutputPath"
Write-Host "Run ID: $RunId"
Write-Host "Runtime started: $RuntimeStarted"
Write-Host "Final run summary observed: $RuntimeEnded"
Write-Host "Package SHA-256: $(Get-UpperSha256 $OutputPath)"
