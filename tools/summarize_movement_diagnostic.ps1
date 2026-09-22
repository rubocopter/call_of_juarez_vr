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

function New-JumpCandidate($Previous, $Current) {
    $InitialY = if ($null -ne $Previous -and $null -ne $Previous.actorY) {
        [double]$Previous.actorY
    } else {
        [double]$Current.actorY
    }
    return [pscustomobject]@{
        startTimestampUs = [double]$Current.timestampUs
        apexTimestampUs = [double]$Current.timestampUs
        initialY = $InitialY
        maximumY = [double]$Current.actorY
        finalY = [double]$Current.actorY
        maxVerticalVelocity = [double]$Current.nativeVerticalSpeed
    }
}

function Update-JumpCandidate($Candidate, $Current) {
    if ($null -eq $Candidate -or $null -eq $Current.actorY) { return }
    $Candidate.finalY = [double]$Current.actorY
    if ([double]$Current.actorY -gt [double]$Candidate.maximumY) {
        $Candidate.maximumY = [double]$Current.actorY
        $Candidate.apexTimestampUs = [double]$Current.timestampUs
    }
    if ($null -ne $Current.nativeVerticalSpeed -and
        [double]$Current.nativeVerticalSpeed -gt [double]$Candidate.maxVerticalVelocity) {
        $Candidate.maxVerticalVelocity = [double]$Current.nativeVerticalSpeed
    }
}

function Get-ReconstructedJumps($Samples, $EmittedJumps) {
    if (@($Samples | Where-Object jumpSignalAvailable).Count -eq 0) {
        return @($EmittedJumps)
    }

    $Result = [Collections.Generic.List[object]]::new()
    $State = "grounded"
    $Previous = $null
    $Candidate = $null
    foreach ($Sample in $Samples) {
        if ($null -eq $Sample.actorY -or $null -eq $Sample.timestampUs) {
            $Previous = $Sample
            continue
        }

        switch ($State) {
            "grounded" {
                if ($Sample.jumping -or -not $Sample.grounded) {
                    $Candidate = New-JumpCandidate $Previous $Sample
                    $State = if ($Sample.jumping) {
                        if ($Sample.nativeVerticalSpeed -gt 0) { "ascending" } else { "descending" }
                    } else { "candidate" }
                }
            }
            "candidate" {
                Update-JumpCandidate $Candidate $Sample
                if ($Sample.jumping) {
                    $State = if ($Sample.nativeVerticalSpeed -gt 0) { "ascending" } else { "descending" }
                } elseif ($Sample.grounded) {
                    # ODE can lose contact briefly on terrain. Without the
                    # game's IsJumping state this was never an accepted jump.
                    $Candidate = $null
                    $State = "grounded"
                }
            }
            "ascending" {
                Update-JumpCandidate $Candidate $Sample
                if ($Sample.grounded) {
                    $State = "landed"
                } elseif ($Sample.nativeVerticalSpeed -le 0) {
                    $State = "descending"
                }
            }
            "descending" {
                Update-JumpCandidate $Candidate $Sample
                if ($Sample.grounded) { $State = "landed" }
            }
            "landed" {
                Update-JumpCandidate $Candidate $Sample
                if (-not $Sample.grounded -and $Sample.jumping) {
                    $State = "descending"
                } elseif ($Sample.grounded -and -not $Sample.jumping) {
                    $Legacy = @($EmittedJumps | Where-Object {
                        $null -ne $_.initialY -and
                        [Math]::Abs([double]$_.initialY - [double]$Candidate.initialY) -le 0.01
                    } | Select-Object -First 1)
                    $EndUs = [double]$Sample.timestampUs
                    $Result.Add([pscustomobject]@{
                        startTimestampUs = [long]$Candidate.startTimestampUs
                        apexTimestampUs = [long]$Candidate.apexTimestampUs
                        endTimestampUs = [long]$EndUs
                        initialY = [Math]::Round([double]$Candidate.initialY, 6)
                        maximumY = [Math]::Round([double]$Candidate.maximumY, 6)
                        finalY = [Math]::Round([double]$Candidate.finalY, 6)
                        apexCm = [Math]::Round(
                            [double]$Candidate.maximumY - [double]$Candidate.initialY, 6)
                        durationSeconds = [Math]::Round(
                            ($EndUs - [double]$Candidate.startTimestampUs) / 1000000.0, 6)
                        timeToApexSeconds = [Math]::Round(
                            ([double]$Candidate.apexTimestampUs -
                                [double]$Candidate.startTimestampUs) / 1000000.0, 6)
                        maxVerticalVelocity = [Math]::Round(
                            [double]$Candidate.maxVerticalVelocity, 6)
                        requestedJumpHeight = if ($Legacy.Count -gt 0) {
                            $Legacy[0].requestedJumpHeight
                        } else { $null }
                        stairHeight = if ($Legacy.Count -gt 0) {
                            $Legacy[0].stairHeight
                        } else { $null }
                    })
                    $Candidate = $null
                    $State = "grounded"
                }
            }
        }
        $Previous = $Sample
    }
    return @($Result)
}

function Get-ForwardBouts($Samples) {
    $Result = [Collections.Generic.List[object]]::new()
    $Current = [Collections.Generic.List[object]]::new()
    foreach ($Sample in $Samples) {
        $FullForward = $null -ne $Sample.forwardSpeed -and
            [Math]::Abs([double]$Sample.forwardSpeed - 1.0) -le 0.001 -and
            -not $Sample.jumping
        if ($FullForward) {
            $Current.Add($Sample)
            continue
        }
        if ($Current.Count -gt 0) {
            $BoutSamples = @($Current)
            $Result.Add([pscustomobject]@{
                samples = $BoutSamples
                startTimestampUs = $BoutSamples[0].timestampUs
                endTimestampUs = $BoutSamples[-1].timestampUs
                trotCount = @($BoutSamples | Where-Object { [int]$_.speedState -eq 129 }).Count
                walkCount = @($BoutSamples | Where-Object { [int]$_.speedState -eq 128 }).Count
            })
            $Current = [Collections.Generic.List[object]]::new()
        }
    }
    if ($Current.Count -gt 0) {
        $BoutSamples = @($Current)
        $Result.Add([pscustomobject]@{
            samples = $BoutSamples
            startTimestampUs = $BoutSamples[0].timestampUs
            endTimestampUs = $BoutSamples[-1].timestampUs
            trotCount = @($BoutSamples | Where-Object { [int]$_.speedState -eq 129 }).Count
            walkCount = @($BoutSamples | Where-Object { [int]$_.speedState -eq 128 }).Count
        })
    }
    return @($Result)
}

function Get-BoutSpeed($Bout) {
    if ($null -eq $Bout) { return $null }
    $Distance = 0.0
    $Duration = 0.0
    foreach ($Sample in $Bout.samples) {
        if ($null -ne $Sample.gameDelta -and $Sample.gameDelta -gt 0) {
            $Distance += [double]$Sample.deltaHorizontal
            $Duration += [double]$Sample.gameDelta
        }
    }
    if ($Duration -le 0) { return $null }
    return $Distance / $Duration
}

function New-Phase([string]$Name) {
    return [ordered]@{
        name = $Name
        startUs = $null
        endUs = $null
        declaredDurationSeconds = $null
        samples = [Collections.Generic.List[object]]::new()
        renders = [Collections.Generic.List[object]]::new()
        emittedJumps = [Collections.Generic.List[object]]::new()
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
        $Actor = Get-Vector $Line "actor"
        $Wanted = Get-Vector $Line "wanted_local_speed"
        $JumpingText = Get-Field $Line "jumping"
        $Phases[$Name].samples.Add([pscustomobject]@{
            timestampUs = Get-Number $Line "timestamp_us"
            gameDelta = Get-Number $Line "game_delta"
            horizontalSpeed = Get-Number $Line "horizontal_speed"
            deltaHorizontal = if ($null -ne $Delta) {
                [Math]::Sqrt($Delta[0] * $Delta[0] + $Delta[2] * $Delta[2])
            } else { 0.0 }
            run = (Get-Field $Line "run") -eq "true"
            canRun = (Get-Field $Line "can_run") -eq "true"
            forwardSpeed = Get-Number $Line "forward_speed"
            speedState = Get-Number $Line "speed_state"
            actorY = if ($null -ne $Actor) { $Actor[1] } else { $null }
            wantedSpeed = if ($null -ne $Wanted) {
                [Math]::Sqrt($Wanted[0] * $Wanted[0] + $Wanted[2] * $Wanted[2])
            } else { $null }
            nativeVerticalSpeed = Get-Number $Line "native_vertical_speed"
            grounded = (Get-Field $Line "grounded") -eq "true"
            jumping = $JumpingText -eq "true"
            jumpSignalAvailable = $null -ne $JumpingText
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
        $Phases[$Name].emittedJumps.Add([pscustomobject]@{
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
    # Keyboard W is a full forward request. The protocol separates its two
    # held-input bouts with a release. Select the last Walk-dominant bout and
    # the preceding Trot-dominant bout, preserving acceleration and gait
    # transitions in each measured input interval.
    $ForwardBouts = @(Get-ForwardBouts $Samples)
    $WalkBout = @($ForwardBouts | Where-Object { $_.walkCount -gt $_.trotCount } |
        Select-Object -Last 1)
    $WalkBout = if ($WalkBout.Count -gt 0) { $WalkBout[0] } else { $null }
    $NormalBout = if ($null -ne $WalkBout) {
        @($ForwardBouts | Where-Object {
            $_.endTimestampUs -lt $WalkBout.startTimestampUs -and
            $_.trotCount -gt $_.walkCount
        } | Select-Object -Last 1)
    } else { @() }
    $NormalBout = if ($NormalBout.Count -gt 0) { $NormalBout[0] } else { $null }
    $NormalMovement = if ($null -ne $NormalBout) { @($NormalBout.samples) } else { @() }
    $WalkModifier = if ($null -ne $WalkBout) { @($WalkBout.samples) } else { @() }
    $Stationary = @($Samples | Where-Object {
        $_.horizontalSpeed -le 1.0 -and
        ($null -eq $_.forwardSpeed -or [Math]::Abs([double]$_.forwardSpeed) -le 0.001)
    })
    $NormalSpeed = Get-BoutSpeed $NormalBout
    $WalkModifierSpeed = Get-BoutSpeed $WalkBout
    $NormalTargetStats = Get-Stats @($NormalMovement |
        Where-Object { $null -ne $_.wantedSpeed } | ForEach-Object wantedSpeed)
    $WalkModifierTargetStats = Get-Stats @($WalkModifier |
        Where-Object { $null -ne $_.wantedSpeed } | ForEach-Object wantedSpeed)
    $NormalTargetSpeed = if ($null -ne $NormalTargetStats) { $NormalTargetStats.p50 } else { $null }
    $WalkModifierTargetSpeed = if ($null -ne $WalkModifierTargetStats) {
        $WalkModifierTargetStats.p50
    } else { $null }
    $Jumps = @(Get-ReconstructedJumps $Samples @($Phase.emittedJumps))
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
        normalMovementNativeState = "trot"
        normalMovementSpeedState = 129
        normalMovementSampleCount = $NormalMovement.Count
        normalMovementStartTimestampUs = if ($null -ne $NormalBout) {
            [long]$NormalBout.startTimestampUs
        } else { $null }
        normalMovementDurationSeconds = if ($null -ne $NormalBout) {
            [Math]::Round(
                ([double]$NormalBout.endTimestampUs - [double]$NormalBout.startTimestampUs) /
                    1000000.0,
                3)
        } else { $null }
        normalMovementCmPerSecond = if ($null -ne $NormalSpeed) {
            [Math]::Round($NormalSpeed, 3)
        } else { $null }
        normalMovementTargetCmPerSecond = if ($null -ne $NormalTargetSpeed) {
            [Math]::Round($NormalTargetSpeed, 3)
        } else { $null }
        walkModifierNativeState = "walk"
        walkModifierSpeedState = 128
        walkModifierSampleCount = $WalkModifier.Count
        walkModifierStartTimestampUs = if ($null -ne $WalkBout) {
            [long]$WalkBout.startTimestampUs
        } else { $null }
        walkModifierDurationSeconds = if ($null -ne $WalkBout) {
            [Math]::Round(
                ([double]$WalkBout.endTimestampUs - [double]$WalkBout.startTimestampUs) /
                    1000000.0,
                3)
        } else { $null }
        walkModifierCmPerSecond = if ($null -ne $WalkModifierSpeed) {
            [Math]::Round($WalkModifierSpeed, 3)
        } else { $null }
        walkModifierTargetCmPerSecond = if ($null -ne $WalkModifierTargetSpeed) {
            [Math]::Round($WalkModifierTargetSpeed, 3)
        } else { $null }
        normalWalkRatio = if ($null -ne $WalkModifierSpeed -and
            $WalkModifierSpeed -gt 0 -and $null -ne $NormalSpeed) {
            [Math]::Round($NormalSpeed / $WalkModifierSpeed, 3)
        } else { $null }
        stationarySampleCount = $Stationary.Count
        runRequestObserved = @($Samples | Where-Object run).Count -gt 0
        canRunObserved = @($Samples | Where-Object canRun).Count -gt 0
        gameUpdates = $Samples.Count
        updateHz = if ($SimulationDuration -gt 0) { [Math]::Round($Samples.Count / $SimulationDuration, 3) } else { $null }
        leftEyeRenders = $LeftCount
        rightEyeRenders = $RightCount
        stereoPairs = $PairCount
        stereoPairHz = if ($WallDuration -gt 0) { [Math]::Round($PairCount / $WallDuration, 3) } else { $null }
        presenterNewSubmissions = $NewSubmissions
        presenterRepeatedSubmissions = $RepeatSubmissions
        jumps = $Jumps
        jumpApexCm = Get-Stats @($Jumps | ForEach-Object apexCm)
        jumpDurationSeconds = Get-Stats @($Jumps | ForEach-Object durationSeconds)
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
    schemaVersion = 2
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
    Write-Host ("{0}: update={1} Hz pair={2} Hz normal={3} cm/s walk-modifier={4} cm/s normal/walk={5} jumps={6}" -f `
        $Phase.phase, $Phase.updateHz, $Phase.stereoPairHz,
        $Phase.normalMovementCmPerSecond, $Phase.walkModifierCmPerSecond,
        $Phase.normalWalkRatio, $Phase.jumps.Count)
}
