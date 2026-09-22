param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,

    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
$Culture = [System.Globalization.CultureInfo]::InvariantCulture
$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$LogPath = Join-Path $GameDirectory "cojvr.log"
if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
    throw "Movement diagnostic log was not found: $LogPath"
}

function Get-Field([string]$Line, [string]$Name) {
    $Match = [regex]::Match($Line, "(?:detail=|[ ;])" + [regex]::Escape($Name) + "=([^;\s]+)")
    if ($Match.Success) { return $Match.Groups[1].Value }
    return $null
}

function Get-Number([string]$Line, [string]$Name) {
    $Text = Get-Field $Line $Name
    if ($null -eq $Text) { return $null }
    $Value = 0.0
    if ([double]::TryParse($Text, [Globalization.NumberStyles]::Float, $Culture, [ref]$Value) -and
        [double]::IsFinite($Value)) { return $Value }
    return $null
}

function Get-Vector([string]$Line, [string]$Name) {
    $Match = [regex]::Match(
        $Line,
        [regex]::Escape($Name) + "=\(([-+0-9.eE]+),([-+0-9.eE]+),([-+0-9.eE]+)\)")
    if (-not $Match.Success) { return $null }
    return @(1..3 | ForEach-Object {
        [double]::Parse($Match.Groups[$_].Value, $Culture)
    })
}

function Get-Stats([double[]]$Values) {
    if ($null -eq $Values -or $Values.Count -eq 0) { return $null }
    $Sorted = @($Values | Sort-Object)
    $Sum = 0.0
    foreach ($Value in $Sorted) { $Sum += $Value }
    return [ordered]@{
        count = $Sorted.Count
        average = [Math]::Round($Sum / $Sorted.Count, 3)
        p50 = [Math]::Round($Sorted[[Math]::Floor(($Sorted.Count - 1) * 0.50)], 3)
        p95 = [Math]::Round($Sorted[[Math]::Floor(($Sorted.Count - 1) * 0.95)], 3)
        max = [Math]::Round($Sorted[-1], 3)
    }
}

function New-Phase([string]$Name) {
    return [ordered]@{
        name = $Name
        startUs = $null
        endUs = $null
        declaredDurationSeconds = $null
        samples = [Collections.Generic.List[object]]::new()
        renders = [Collections.Generic.List[object]]::new()
        jumps = [Collections.Generic.List[object]]::new()
        readback = [Collections.Generic.List[double]]::new()
        copy = [Collections.Generic.List[double]]::new()
    }
}

$Phases = [ordered]@{}
$ActivePhase = $null
$Lines = Get-Content -LiteralPath $LogPath
foreach ($Line in $Lines) {
    if ($Line -match "^camera_probe_event: event=movement_trace_phase result=start ") {
        $Name = [string](Get-Field $Line "phase")
        if ([string]::IsNullOrWhiteSpace($Name)) { continue }
        if (-not $Phases.Contains($Name)) { $Phases[$Name] = New-Phase $Name }
        $ActivePhase = $Phases[$Name]
        $ActivePhase.startUs = Get-Number $Line "timestamp_us"
        continue
    }
    if ($Line -match "^camera_probe_event: event=movement_trace_phase result=end ") {
        $Name = [string](Get-Field $Line "phase")
        if ($Phases.Contains($Name)) {
            $Phases[$Name].endUs = Get-Number $Line "timestamp_us"
            $Phases[$Name].declaredDurationSeconds = Get-Number $Line "duration_s"
        }
        $ActivePhase = $null
        continue
    }
    if ($Line -match "^camera_probe_event: event=movement_trace_sample result=ok ") {
        $Name = [string](Get-Field $Line "phase")
        if (-not $Phases.Contains($Name)) { $Phases[$Name] = New-Phase $Name }
        $Delta = Get-Vector $Line "delta"
        $Phases[$Name].samples.Add([pscustomobject]@{
            timestampUs = Get-Number $Line "timestamp_us"
            gameDelta = Get-Number $Line "game_delta"
            horizontalSpeed = Get-Number $Line "horizontal_speed"
            deltaHorizontal = if ($null -ne $Delta) {
                [Math]::Sqrt($Delta[0] * $Delta[0] + $Delta[2] * $Delta[2])
            } else { 0.0 }
            run = (Get-Field $Line "run") -eq "true"
            grounded = (Get-Field $Line "grounded") -eq "true"
        })
        continue
    }
    if ($Line -match "^camera_probe_event: event=movement_render_sample result=ok ") {
        $Name = [string](Get-Field $Line "phase")
        if (-not $Phases.Contains($Name)) { $Phases[$Name] = New-Phase $Name }
        $Phases[$Name].renders.Add([pscustomobject]@{
            timestampUs = Get-Number $Line "timestamp_us"
            left = (Get-Field $Line "left_rendered") -eq "true"
            right = (Get-Field $Line "right_rendered") -eq "true"
            submitted = (Get-Field $Line "submitted") -eq "true"
            framesUploaded = Get-Number $Line "frames_uploaded"
            newSubmissions = Get-Number $Line "new_submissions"
            repeatSubmissions = Get-Number $Line "repeat_submissions"
        })
        continue
    }
    if ($Line -match "^camera_probe_event: event=movement_jump_summary result=complete ") {
        $Name = [string](Get-Field $Line "phase")
        if (-not $Phases.Contains($Name)) { $Phases[$Name] = New-Phase $Name }
        $Phases[$Name].jumps.Add([pscustomobject]@{
            initialY = Get-Number $Line "start_y"
            maximumY = Get-Number $Line "max_y"
            finalY = Get-Number $Line "final_y"
            apexCm = Get-Number $Line "apex_height_cm"
            durationSeconds = Get-Number $Line "duration_s"
            timeToApexSeconds = Get-Number $Line "time_to_apex_s"
            maxVerticalVelocity = Get-Number $Line "max_vertical_velocity"
            requestedJumpHeight = Get-Number $Line "requested_jump_height"
            stairHeight = Get-Number $Line "stair_height"
        })
        continue
    }
    if ($null -ne $ActivePhase -and
        $Line -match "^native_stereo_producer_timing: status=published ") {
        $Readback = Get-Number $Line "deferred_readback_ms"
        $Copy = Get-Number $Line "cpu_copy_ms"
        if ($null -ne $Readback) { $ActivePhase.readback.Add($Readback) }
        if ($null -ne $Copy) { $ActivePhase.copy.Add($Copy) }
    }
}

$Results = [Collections.Generic.List[object]]::new()
foreach ($Entry in $Phases.GetEnumerator()) {
    $Phase = $Entry.Value
    $Samples = @($Phase.samples)
    $Renders = @($Phase.renders)
    $SimulationDuration = 0.0
    $Distance = 0.0
    foreach ($Sample in $Samples) {
        if ($null -ne $Sample.gameDelta -and $Sample.gameDelta -gt 0) {
            $SimulationDuration += $Sample.gameDelta
        }
        $Distance += $Sample.deltaHorizontal
    }
    $Moving = @($Samples | Where-Object { $_.horizontalSpeed -gt 1.0 })
    $Walk = @($Moving | Where-Object { -not $_.run })
    $Sprint = @($Moving | Where-Object { $_.run })
    $WalkSpeed = if ($Walk.Count -gt 0) { ($Walk | Measure-Object horizontalSpeed -Average).Average } else { $null }
    $SprintSpeed = if ($Sprint.Count -gt 0) { ($Sprint | Measure-Object horizontalSpeed -Average).Average } else { $null }
    $WallDuration = if ($null -ne $Phase.startUs -and $null -ne $Phase.endUs) {
        ($Phase.endUs - $Phase.startUs) / 1000000.0
    } elseif ($null -ne $Phase.declaredDurationSeconds) {
        $Phase.declaredDurationSeconds
    } elseif ($Renders.Count -gt 1) {
        ($Renders[-1].timestampUs - $Renders[0].timestampUs) / 1000000.0
    } else { $SimulationDuration }
    $LeftCount = @($Renders | Where-Object left).Count
    $RightCount = @($Renders | Where-Object right).Count
    $PairCount = @($Renders | Where-Object { $_.left -and $_.right }).Count
    $NewSubmissions = if ($Renders.Count -gt 1) {
        [Math]::Max(0, $Renders[-1].newSubmissions - $Renders[0].newSubmissions)
    } else { 0 }
    $RepeatSubmissions = if ($Renders.Count -gt 1) {
        [Math]::Max(0, $Renders[-1].repeatSubmissions - $Renders[0].repeatSubmissions)
    } else { 0 }
    $MaxSpeed = if ($Moving.Count -gt 0) {
        ($Moving | Measure-Object horizontalSpeed -Maximum).Maximum
    } else { 0.0 }

    $Results.Add([ordered]@{
        phase = $Phase.name
        durationSeconds = [Math]::Round($WallDuration, 3)
        simulationSeconds = [Math]::Round($SimulationDuration, 3)
        distanceCm = [Math]::Round($Distance, 3)
        horizontalSpeedMeanCmPerSecond = if ($SimulationDuration -gt 0) {
            [Math]::Round($Distance / $SimulationDuration, 3)
        } else { $null }
        horizontalSpeedMaxCmPerSecond = [Math]::Round($MaxSpeed, 3)
        walkCmPerSecond = if ($null -ne $WalkSpeed) { [Math]::Round($WalkSpeed, 3) } else { $null }
        sprintCmPerSecond = if ($null -ne $SprintSpeed) { [Math]::Round($SprintSpeed, 3) } else { $null }
        sprintWalkRatio = if ($null -ne $WalkSpeed -and $WalkSpeed -gt 0 -and $null -ne $SprintSpeed) {
            [Math]::Round($SprintSpeed / $WalkSpeed, 3)
        } else { $null }
        gameUpdates = $Samples.Count
        updateHz = if ($SimulationDuration -gt 0) { [Math]::Round($Samples.Count / $SimulationDuration, 3) } else { $null }
        leftEyeRenders = $LeftCount
        rightEyeRenders = $RightCount
        stereoPairs = $PairCount
        stereoPairHz = if ($WallDuration -gt 0) { [Math]::Round($PairCount / $WallDuration, 3) } else { $null }
        presenterNewSubmissions = $NewSubmissions
        presenterRepeatedSubmissions = $RepeatSubmissions
        jumps = @($Phase.jumps)
        jumpApexCm = Get-Stats @($Phase.jumps | ForEach-Object apexCm)
        jumpDurationSeconds = Get-Stats @($Phase.jumps | ForEach-Object durationSeconds)
        readbackMs = Get-Stats @($Phase.readback)
        cpuCopyMs = Get-Stats @($Phase.copy)
    })
}

$RunId = "unbound"
$RunManifestPath = Join-Path $GameDirectory ".cojvr-run.json"
if (Test-Path -LiteralPath $RunManifestPath -PathType Leaf) {
    $RunId = [string]((Get-Content -LiteralPath $RunManifestPath -Raw | ConvertFrom-Json).runId)
}
$Report = [ordered]@{
    schemaVersion = 1
    manifestType = "cojvr-movement-diagnostic-summary"
    runId = $RunId
    generatedUtc = [DateTime]::UtcNow.ToString("o")
    phases = @($Results)
}
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $GameDirectory "cojvr-movement-summary.json"
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)
[IO.File]::WriteAllText(
    $OutputPath,
    ($Report | ConvertTo-Json -Depth 10) + [Environment]::NewLine,
    [Text.UTF8Encoding]::new($false))

Write-Host "Movement diagnostic summary: $OutputPath"
foreach ($Phase in $Results) {
    Write-Host ("{0}: update={1} Hz pair={2} Hz walk={3} cm/s sprint={4} cm/s ratio={5} jumps={6}" -f `
        $Phase.phase, $Phase.updateHz, $Phase.stereoPairHz, $Phase.walkCmPerSecond,
        $Phase.sprintCmPerSecond, $Phase.sprintWalkRatio, $Phase.jumps.Count)
}
