param([string]$SourceDirectory = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
function Assert($Condition, $Message) { if (-not $Condition) { throw $Message } }
$Root = Join-Path ([IO.Path]::GetTempPath()) ('cojvr-summary-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $Root | Out-Null
try {
    # Synthetic provenance only: never invoke staging or the physical workflow.
    $RunId = 'summary-fixture'
    $BuildId = 'a' * 64
    $Evidence = Join-Path $Root '.cojvr-evidence/runs/summary-fixture'
    New-Item -ItemType Directory -Path $Evidence -Force | Out-Null
    $Manifest = Join-Path $Evidence 'build-manifest.json'
    @{ manifestId = $BuildId } | ConvertTo-Json | Set-Content $Manifest
    $Run = @{ runId = $RunId; buildManifestId = $BuildId
        buildManifestSha256 = (Get-FileHash $Manifest -Algorithm SHA256).Hash
        diagnosticMode = 'd3d9_native_stereo'; deployment = @() }
    foreach ($Name in @('.cojvr-run.json', '.cojvr-d3d9-stage.json')) {
        $Run | ConvertTo-Json | Set-Content (Join-Path $Root $Name)
    }
    function Summarize([string[]]$Rows) {
        @("run_start: run_id=$RunId build_manifest_id=$BuildId pid=123") + $Rows +
            @("run_end: run_id=$RunId") | Set-Content (Join-Path $Root 'cojvr.log')
        $script:Console = & (Join-Path $SourceDirectory 'tools/summarize_native_stereo_run.ps1') `
            -GameDirectory $Root 6>&1 | Out-String
        Get-Content (Join-Path $Root 'cojvr-native-stereo-summary.json') -Raw | ConvertFrom-Json
    }
    $Rows = @(100..0 | ForEach-Object { "native_stereo_presenter_frame: status=new frame_age_ms=$_" })
    $Report = Summarize $Rows
    $Stats = $Report.metricsMs.frameAge
    Assert ($Stats.p99 -eq 99) 'p99 must expose the 99th floor-ranked sample.'
    Assert ($Stats.count -eq 101 -and $Stats.min -eq 0 -and $Stats.average -eq 50 -and
        $Stats.p50 -eq 50 -and $Stats.p95 -eq 95 -and $Stats.max -eq 100) 'Legacy statistics changed.'
    Assert ($Console.Contains('p95=95 ms p99=99 ms max=100 ms')) 'Console omits or mislabels p99.'
    Assert ($Report.schemaVersion -eq 2 -and $Report.runId -eq $RunId -and
        $Report.buildManifestId -eq $BuildId) 'Additive metric changed provenance/schema.'
    $Report = Summarize @('native_stereo_presenter_timing: status=ok submit_ms=2.12349')
    Assert ($Report.metricsMs.submit.p99 -eq 2.123 -and $null -eq $Report.metricsMs.frameAge) `
        'Single rounded sample or absent metric changed.'
    $Report = Summarize @('native_stereo_presenter_timing: status=ok submit_ms=1',
        'native_stereo_presenter_timing: status=ok submit_ms=1000',
        'native_stereo_presenter_timing: status=failed submit_ms=9999',
        'native_stereo_presenter_timing: status=ok submit_ms=1e999')
    Assert ($Report.metricsMs.submit.count -eq 2 -and $Report.metricsMs.submit.p99 -eq 1 -and
        $Report.metricsMs.submit.max -eq 1000) 'Small samples must retain the existing floor convention/filter.'
    $Report = Summarize @(
        'camera_probe_event: event=camera_hmd_orientation_applied result=ok head_position_valid=true relative_head_position=(3,4,0)',
        'camera_probe_event: event=camera_hmd_orientation_applied result=ok head_position_valid=false relative_head_position=(0,0,999)')
    Assert ($Report.motion.headTranslationMetres.count -eq 1 -and
        $Report.motion.headTranslationMetres.p50 -eq 5 -and
        $Report.motion.headTranslationMetres.p95 -eq 5 -and
        $Report.motion.headTranslationMetres.p99 -eq 5 -and
        $Report.motion.headTranslationMetres.max -eq 5) 'Shared statistics lost motion p99 or admitted invalid head position.'
    $Report = Summarize @()
    Assert ($null -eq $Report.metricsMs.submit -and $null -eq $Report.motion.headTranslationMetres) `
        'Legacy logs with no timing metrics must remain readable.'
    Write-Host 'PASS native stereo summary: percentile tail, legacy statistics, console, provenance, singleton, filtering, motion, empty logs'
} finally {
    # Only remove the exact, freshly-created fixture folder under the system temp root.
    $FullRoot = [IO.Path]::GetFullPath($Root)
    $TempParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $FullRoot.StartsWith($TempParent, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($FullRoot) -notlike 'cojvr-summary-*') { throw 'Unsafe fixture cleanup path.' }
    Remove-Item -LiteralPath $FullRoot -Recurse -Force
}
