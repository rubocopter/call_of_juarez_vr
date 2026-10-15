param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,

    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "coj_validation_result.ps1")

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
$ValidationContext = Get-CoJValidationContext $GameDirectory
$ValidationResult = $null
if (Test-Path -LiteralPath $ValidationContext.ResultPath -PathType Leaf) {
    $ValidationResult = Get-Content -LiteralPath $ValidationContext.ResultPath -Raw | ConvertFrom-Json
}
# Recollection can refresh an explicit absence of verification, never an
# executed verdict. Late logs still cannot turn collection into acceptance.
if ($null -eq $ValidationResult -or ($ValidationResult.schemaVersion -eq 1 -and
    $ValidationResult.manifestType -eq 'cojvr-validation-result' -and
    $ValidationResult.outcome -eq 'inconclusive' -and
    $ValidationResult.verifier.executed -eq $false -and $ValidationResult.verifier.succeeded -eq $false -and
    @($ValidationResult.reasons).Count -eq 1 -and $ValidationResult.reasons[0].code -eq 'verifier_not_executed')) {
    Write-CoJValidationResult $ValidationContext $false $false @([ordered]@{
        code = 'verifier_not_executed'; message = 'No canonical verifier result was recorded for these run inputs.'
    })
}
$ValidationResult = Get-Content -LiteralPath $ValidationContext.ResultPath -Raw | ConvertFrom-Json
Assert-CoJValidationResult $ValidationContext $ValidationResult
# Files already under the run directory still require inventory hashes.
foreach ($Relative in @('analysis/validation-result.json', 'build-manifest.json', 'operator-observations.json')) {
    $Path = Join-Path $RunDirectory $Relative
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        $Inventory.Add([ordered]@{path=$Relative; sha256=Get-UpperSha256 $Path; size=(Get-Item -LiteralPath $Path).Length})
    }
}
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
$InvalidNativeInterval = [string]$CurrentRun.diagnosticMode -eq 'd3d9_native_stereo' -and
    -not $ValidationContext.RuntimeMarkersValid
# A rejected process interval may prevent summary generation. Preserve its raw
# log and verdict anyway; the verdict remains non-passing and incomplete.
if ($RequirePerformanceSummary -and -not $InvalidNativeInterval -and
    -not (Test-Path -LiteralPath $NativeStereoSummary -PathType Leaf)) {
    throw "This run requires a native-stereo performance/state summary before evidence collection."
}
Copy-EvidenceFile $RuntimeLog (Join-Path $RunDirectory "runtime\cojvr.log") $Inventory
# Recollection must not pack a former report after its current identity was
# rejected. Preserve previous bytes outside this package's directory; copying
# an admitted current report below creates a fresh, inventoried destination.
$RuntimeDirectory = Join-Path $RunDirectory 'runtime'
$PreviousFaults = @()
if (Test-Path -LiteralPath $RuntimeDirectory -PathType Container) {
    $PreviousFaults = @(Get-ChildItem -LiteralPath $RuntimeDirectory -File -Filter 'cojvr-render-fault-*.txt')
}
foreach ($PreviousFault in $PreviousFaults) {
    if ($PreviousFault.Name -cnotmatch '^cojvr-render-fault-[0-9]+\.txt$') { continue }
    $PreviousHash = Get-UpperSha256 $PreviousFault.FullName
    $RetiredDirectory = Join-Path (Join-Path $EvidenceRoot 'retired-diagnostics') $RunId
    if (-not (Test-Path -LiteralPath $RetiredDirectory -PathType Container)) {
        New-Item -ItemType Directory -Path $RetiredDirectory -Force | Out-Null
    }
    $RetiredPath = Join-Path $RetiredDirectory ($PreviousFault.BaseName + '-' + $PreviousHash + '.txt')
    if (Test-Path -LiteralPath $RetiredPath -PathType Leaf) {
        if ((Get-UpperSha256 $RetiredPath) -cne $PreviousHash) { throw 'Retired diagnostic preservation hash mismatch.' }
        Remove-Item -LiteralPath $PreviousFault.FullName
    } else {
        Move-Item -LiteralPath $PreviousFault.FullName -Destination $RetiredPath
    }
}
# Render-fault reports are diagnostic only. Bind the optional file to the one
# native process and exact run/build; a stale or malformed report cannot become
# evidence for this candidate or prevent ordinary collection/restoration.
if ([string]$CurrentRun.diagnosticMode -eq 'd3d9_native_stereo' -and $ValidationContext.RuntimeMarkersValid) {
    $ProcessMarkers = @($ValidationContext.RuntimeLines | Where-Object {
        $_ -cmatch '^run_start: .* pid=([0-9]+)$'
    })
    if ($ProcessMarkers.Count -eq 1 -and $ProcessMarkers[0] -cmatch ' pid=([0-9]+)$') {
        $NativeProcessId = $Matches[1]
        $FaultName = "cojvr-render-fault-$NativeProcessId.txt"
        $FaultPath = Join-Path $GameDirectory $FaultName
        if (Test-Path -LiteralPath $FaultPath -PathType Leaf) {
            try {
                $Fault = Get-Item -LiteralPath $FaultPath
                if ($Fault.Length -gt 16384) { throw 'Oversized render-fault report.' }
                $FaultText = Get-Content -LiteralPath $FaultPath -Raw
                $Expected = [ordered]@{schema_version='1'; scope='coj_render_hook_escape'; run_id=$RunId;
                    build_manifest_id=[string]$CurrentRun.buildManifestId; pid=$NativeProcessId}
                foreach ($Key in $Expected.Keys) {
                    $Values = [regex]::Matches($FaultText, '(?m)^' + [regex]::Escape($Key) + '=([^\r\n]*)\r?$')
                    if ($Values.Count -ne 1 -or $Values[0].Groups[1].Value -cne $Expected[$Key]) {
                        throw "Render-fault identity mismatch: $Key."
                    }
                }
                Copy-EvidenceFile $FaultPath (Join-Path $RunDirectory "runtime\$FaultName") $Inventory
            } catch { Write-Warning "Optional render-fault report excluded: $($_.Exception.Message)" }
        }
    }
}
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

if ([string]$CurrentRun.diagnosticMode -eq 'd3d9_native_stereo') {
    # Use the same ordered, exact run/build interval as the canonical verdict.
    # Structured summaries cannot repair a foreign, duplicated or reversed end.
    $RuntimeStarted = $ValidationContext.RuntimeStarted
    $RuntimeEnded = $ValidationContext.RuntimeEnded
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
# The package must describe the same inputs as the verdict, including the
# bytes copied above. Fail closed if they changed during collection.
Assert-CoJValidationResult (Get-CoJValidationContext $GameDirectory) $ValidationResult
$EvidenceManifest = [ordered]@{
    schemaVersion = 1
    manifestType = "cojvr-run-evidence"
    runId = $RunId
    buildManifestId = [string]$CurrentRun.buildManifestId
    collectedUtc = [DateTime]::UtcNow.ToString("o")
    runtimeStarted = $RuntimeStarted
    runtimeEnded = $RuntimeEnded
    incomplete = -not $RuntimeEnded
    validation = [ordered]@{
        resultPath = 'analysis/validation-result.json'
        outcome = $ValidationResult.outcome
        validationProfile = $ValidationResult.validationProfile
        scope = 'canonical_telemetry_checks_only'
        headsetAcceptance = 'not_evaluated'
    }
    operatorObservations = [ordered]@{
        path = if (Test-Path -LiteralPath (Join-Path $RunDirectory 'operator-observations.json')) { 'operator-observations.json' } else { $null }
        affectsVerifierOutcome = $false
    }
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
