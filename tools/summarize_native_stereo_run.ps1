param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,

    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
$InvariantCulture = [System.Globalization.CultureInfo]::InvariantCulture

function Get-NumericValues(
    [string[]]$Lines,
    [string]$LinePattern,
    [string]$Field) {
    $Values = [System.Collections.Generic.List[double]]::new()
    $FieldPattern = "(?:^|[ ;])" + [Regex]::Escape($Field) + "=([-+0-9.eE]+)"
    foreach ($Line in $Lines) {
        if ($Line -notmatch $LinePattern) { continue }
        $Match = [regex]::Match($Line, $FieldPattern)
        if (-not $Match.Success) { continue }
        $Value = 0.0
        if ([double]::TryParse(
                $Match.Groups[1].Value,
                [System.Globalization.NumberStyles]::Float,
                $InvariantCulture,
                [ref]$Value) -and [double]::IsFinite($Value)) {
            $Values.Add($Value)
        }
    }
    return @($Values)
}

function Get-Stats([double[]]$Values) {
    if ($null -eq $Values -or $Values.Count -eq 0) { return $null }
    $Sorted = @($Values | Sort-Object)
    $Sum = 0.0
    foreach ($Value in $Sorted) { $Sum += $Value }
    $P50Index = [Math]::Floor(($Sorted.Count - 1) * 0.50)
    $P95Index = [Math]::Floor(($Sorted.Count - 1) * 0.95)
    return [ordered]@{
        count = $Sorted.Count
        min = [Math]::Round($Sorted[0], 3)
        average = [Math]::Round($Sum / $Sorted.Count, 3)
        p50 = [Math]::Round($Sorted[$P50Index], 3)
        p95 = [Math]::Round($Sorted[$P95Index], 3)
        max = [Math]::Round($Sorted[-1], 3)
    }
}

function Get-IntegerField([string]$Line, [string]$Field) {
    if ([string]::IsNullOrWhiteSpace($Line)) { return $null }
    $Match = [regex]::Match(
        $Line,
        "(?:^|[ ;])" + [Regex]::Escape($Field) + "=([0-9]+)")
    if (-not $Match.Success) { return $null }
    return [uint64]::Parse($Match.Groups[1].Value, $InvariantCulture)
}

$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$Provenance = & (Join-Path $PSScriptRoot "get_run_provenance.ps1") `
    -GameDirectory $GameDirectory `
    -ExpectedDiagnosticMode "d3d9_native_stereo"
$RunId = [string]$Provenance.RunId
$Lines = @($Provenance.Lines)

$RuntimeStateLines = @($Lines | Where-Object { $_ -match "^openvr_runtime_state:" })
$PresentingObserved = [bool]($RuntimeStateLines -match ";presenting=true;")
$TrackingObserved = [bool]($RuntimeStateLines -match ";tracking_valid=true;")
$FocusCycleObserved = $false
$ActiveIndex = -1
$LostIndex = -1
for ($Index = 0; $Index -lt $RuntimeStateLines.Count; ++$Index) {
    $Line = $RuntimeStateLines[$Index]
    if ($ActiveIndex -lt 0 -and
        $Line -match ";focused=true;tracking_valid=true;presenting=true;") {
        $ActiveIndex = $Index
        continue
    }
    if ($ActiveIndex -ge 0 -and $LostIndex -lt 0 -and
        $Line -match ";focused=false;") {
        $LostIndex = $Index
        continue
    }
    if ($LostIndex -ge 0 -and
        $Line -match ";focused=true;tracking_valid=true;") {
        $FocusCycleObserved = $true
        break
    }
}

$TransportLine = @($Lines | Where-Object {
    $_ -match "^native_stereo_transport_summary:"
} | Select-Object -Last 1)
$TransportText = if ($TransportLine.Count -gt 0) { [string]$TransportLine[0] } else { "" }

$Report = [ordered]@{
    schemaVersion = 1
    manifestType = "cojvr-native-stereo-summary"
    runId = $RunId
    buildManifestId = [string]$Provenance.BuildManifestId
    generatedUtc = [DateTime]::UtcNow.ToString("o")
    metricsMs = [ordered]@{
        gpuCopyQueue = Get-Stats (Get-NumericValues $Lines "^native_stereo_capture_timing: status=ok" "gpu_copy_queue_ms")
        fencePoll = Get-Stats (Get-NumericValues $Lines "^native_stereo_producer_timing: status=published" "fence_poll_ms")
        deferredReadback = Get-Stats (Get-NumericValues $Lines "^native_stereo_producer_timing: status=published" "deferred_readback_ms")
        cpuCopy = Get-Stats (Get-NumericValues $Lines "^native_stereo_producer_timing: status=published" "cpu_copy_ms")
        producerCollect = Get-Stats (Get-NumericValues $Lines "^native_stereo_producer_timing: status=published" "producer_collect_ms")
        diagnosticHash = Get-Stats (Get-NumericValues $Lines "^native_stereo_presenter_frame: status=new" "hash_ms")
        d3d11Upload = Get-Stats (Get-NumericValues $Lines "^native_stereo_presenter_frame: status=new" "upload_ms")
        waitPose = Get-Stats (Get-NumericValues $Lines "^native_stereo_presenter_timing: status=ok" "wait_pose_ms")
        submit = Get-Stats (Get-NumericValues $Lines "^native_stereo_presenter_timing: status=ok" "submit_ms")
    }
    transport = [ordered]@{
        framesFenced = Get-IntegerField $TransportText "frames_fenced"
        framesCollected = Get-IntegerField $TransportText "frames_collected"
        captureRingDrops = Get-IntegerField $TransportText "capture_ring_drops"
        mailboxPublished = Get-IntegerField $TransportText "mailbox_published"
        mailboxReplaced = Get-IntegerField $TransportText "mailbox_replaced"
        framesUploaded = Get-IntegerField $TransportText "frames_uploaded"
        newSubmissions = Get-IntegerField $TransportText "new_submissions"
        repeatSubmissions = Get-IntegerField $TransportText "repeat_submissions"
        submitFailures = Get-IntegerField $TransportText "submit_failures"
    }
    runtime = [ordered]@{
        stateSamples = $RuntimeStateLines.Count
        trackingValidObserved = $TrackingObserved
        presentingObserved = $PresentingObserved
        focusCycleObserved = $FocusCycleObserved
        shutdownCompleteObserved = [bool]($RuntimeStateLines -match ";lifecycle=shutdown_complete;")
    }
}

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $GameDirectory "cojvr-native-stereo-summary.json"
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)
[System.IO.File]::WriteAllText(
    $OutputPath,
    ($Report | ConvertTo-Json -Depth 10) + [Environment]::NewLine,
    [System.Text.UTF8Encoding]::new($false))

Write-Host "Native-stereo performance/state summary: $OutputPath"
foreach ($MetricName in @("deferredReadback", "cpuCopy", "producerCollect", "d3d11Upload", "waitPose", "submit")) {
    $Stats = $Report.metricsMs[$MetricName]
    if ($null -eq $Stats) { continue }
    Write-Host ("{0}: n={1} avg={2} ms p50={3} ms p95={4} ms max={5} ms" -f `
        $MetricName, $Stats.count, $Stats.average, $Stats.p50, $Stats.p95, $Stats.max)
}
Write-Host "OpenVR focus cycle observed: $($FocusCycleObserved.ToString().ToLowerInvariant())"
Write-Host "OpenVR shutdown-complete state observed: $($Report.runtime.shutdownCompleteObserved.ToString().ToLowerInvariant())"
