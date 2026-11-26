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

    # Every sampled successful submission has a companion pose record. Match
    # capture and render-pose identities, excluding theater and invalid clocks.
    $Report = Summarize @(
        'native_stereo_presenter_frame: status=new frame_age_ms=7',
        'native_stereo_presenter_timing: status=ok submit_sequence=1;capture_sequence=10;render_pose_sequence=71;presentation_mode=native_stereo;content=new',
        'native_stereo_pose_telemetry: status=ok;frame_sequence=10;render_pose_sequence=71;presentation_mode=native_stereo;pose_age_ms=18.25;capture_time_valid=true',
        'native_stereo_presenter_timing: status=ok submit_sequence=2;capture_sequence=10;render_pose_sequence=71;presentation_mode=native_stereo;content=repeated',
        'native_stereo_pose_telemetry: status=ok;frame_sequence=10;render_pose_sequence=71;presentation_mode=native_stereo;pose_age_ms=63.5;capture_time_valid=true',
        'native_stereo_presenter_timing: status=ok submit_sequence=3;capture_sequence=11;render_pose_sequence=72;presentation_mode=native_stereo;content=new',
        'native_stereo_pose_telemetry: status=ok;frame_sequence=11;render_pose_sequence=72;presentation_mode=native_stereo;pose_age_ms=12.5;capture_time_valid=true',
        'native_stereo_presenter_timing: status=ok submit_sequence=4;capture_sequence=11;render_pose_sequence=72;presentation_mode=native_stereo;content=repeated',
        'native_stereo_pose_telemetry: status=ok;frame_sequence=11;render_pose_sequence=72;presentation_mode=native_stereo;pose_age_ms=120.5;capture_time_valid=true',
        'native_stereo_presenter_timing: status=ok submit_sequence=5;capture_sequence=12;render_pose_sequence=73;presentation_mode=flat_theater;content=new',
        'native_stereo_pose_telemetry: status=ok;frame_sequence=12;render_pose_sequence=73;presentation_mode=flat_theater;pose_age_ms=999;capture_time_valid=true',
        'native_stereo_presenter_timing: status=failed submit_sequence=6;capture_sequence=13;render_pose_sequence=74;presentation_mode=native_stereo;content=new',
        'native_stereo_pose_telemetry: status=ok;frame_sequence=13;render_pose_sequence=74;presentation_mode=native_stereo;pose_age_ms=999;capture_time_valid=true',
        'native_stereo_presenter_timing: status=ok submit_sequence=7;capture_sequence=14;render_pose_sequence=75;presentation_mode=native_stereo;content=new',
        'native_stereo_pose_telemetry: status=ok;frame_sequence=14;render_pose_sequence=76;presentation_mode=native_stereo;pose_age_ms=999;capture_time_valid=true',
        'native_stereo_presenter_timing: status=ok submit_sequence=8;capture_sequence=15;render_pose_sequence=77;presentation_mode=native_stereo;content=repeated',
        'native_stereo_pose_telemetry: status=ok;frame_sequence=16;render_pose_sequence=77;presentation_mode=native_stereo;pose_age_ms=999;capture_time_valid=true',
        'native_stereo_presenter_timing: status=ok submit_sequence=9;capture_sequence=17;render_pose_sequence=78;presentation_mode=native_stereo;content=repeated',
        'native_stereo_pose_telemetry: status=ok;frame_sequence=17;render_pose_sequence=78;presentation_mode=native_stereo;pose_age_ms=999;capture_time_valid=false',
        'native_stereo_presenter_timing: status=ok submit_sequence=10;capture_sequence=18;render_pose_sequence=79;presentation_mode=native_stereo;content=repeated',
        'native_stereo_pose_telemetry: status=ok;frame_sequence=18;render_pose_sequence=79;presentation_mode=native_stereo;pose_age_ms=-1;capture_time_valid=true',
        'native_stereo_presenter_timing: status=ok submit_sequence=11;capture_sequence=19;render_pose_sequence=80;presentation_mode=native_stereo;content=repeated',
        'native_stereo_pose_telemetry: status=ok;frame_sequence=19;render_pose_sequence=80;presentation_mode=native_stereo;pose_age_ms=1e999;capture_time_valid=true',
        'native_stereo_presenter_timing: status=ok submit_sequence=12;capture_sequence=18446744073709551616;render_pose_sequence=81;presentation_mode=native_stereo;content=new',
        'native_stereo_pose_telemetry: status=ok;frame_sequence=20;render_pose_sequence=81;presentation_mode=native_stereo;pose_age_ms=999;capture_time_valid=true',
        'native_stereo_presenter_timing: status=ok submit_sequence=12;capture_sequence=20garbage;render_pose_sequence=81;presentation_mode=native_stereo;content=new',
        'native_stereo_pose_telemetry: status=ok;frame_sequence=20;render_pose_sequence=81;presentation_mode=native_stereo;pose_age_ms=999;capture_time_valid=true',
        'native_stereo_presenter_timing: status=ok submit_sequence=12;capture_sequence=20;render_pose_sequence=81;presentation_mode=native_stereo;content=new',
        'native_stereo_pose_telemetry: status=ok;frame_sequence=20;render_pose_sequence=81;presentation_mode=native_stereo;pose_age_ms=999ms;capture_time_valid=true',
        'native_stereo_presenter_timing: status=ok submit_sequence=13;capture_sequence=21;render_pose_sequence=82;presentation_mode=native_stereo;content=new',
        'native_stereo_presenter_timing: status=ok submit_sequence=14;capture_sequence=22;render_pose_sequence=83;presentation_mode=native_stereo;content=repeated',
        'native_stereo_pose_telemetry: status=ok;frame_sequence=21;render_pose_sequence=82;presentation_mode=native_stereo;pose_age_ms=999;capture_time_valid=true',
        # A later presenter session can reuse frame and submission sequence IDs.
        'native_stereo_presenter_timing: status=ok submit_sequence=1;capture_sequence=10;render_pose_sequence=99;presentation_mode=native_stereo;content=new',
        'native_stereo_pose_telemetry: status=ok;frame_sequence=10;render_pose_sequence=99;presentation_mode=native_stereo;pose_age_ms=14.5;capture_time_valid=true')
    $Fresh = $Report.metricsMs.newSubmissionFrameAge
    $Repeated = $Report.metricsMs.repeatedSubmissionFrameAge
    Assert ($Report.metricsMs.frameAge.count -eq 1 -and $Report.metricsMs.frameAge.p99 -eq 7) `
        'Existing upload-age metric changed when sampled submission ages were added.'
    Assert ($Fresh.count -eq 3 -and $Fresh.min -eq 12.5 -and $Fresh.p50 -eq 14.5 -and
        $Fresh.max -eq 18.25) 'Fresh submission frame ages were not joined by identity.'
    Assert ($Repeated.count -eq 2 -and $Repeated.min -eq 63.5 -and $Repeated.max -eq 120.5) `
        'Repeated submission frame ages included invalid, foreign or theater records.'
    Assert ($Console.Contains('repeatedSubmissionFrameAge: n=2')) `
        'Repeated submission frame ages are missing from the console report.'

    $Report = Summarize @()
    Assert ($null -eq $Report.metricsMs.submit -and $null -eq $Report.motion.headTranslationMetres -and
        $null -eq $Report.metricsMs.newSubmissionFrameAge -and
        $null -eq $Report.metricsMs.repeatedSubmissionFrameAge) `
        'Legacy logs with no timing metrics must remain readable.'
    Write-Host 'PASS native stereo summary: percentile tail, legacy statistics, console, provenance, singleton, filtering, motion, paired submission age, empty logs'
} finally {
    # Only remove the exact, freshly-created fixture folder under the system temp root.
    $FullRoot = [IO.Path]::GetFullPath($Root)
    $TempParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $FullRoot.StartsWith($TempParent, [StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($FullRoot) -notlike 'cojvr-summary-*') { throw 'Unsafe fixture cleanup path.' }
    Remove-Item -LiteralPath $FullRoot -Recurse -Force
}
