param(
    [ValidateSet("prepare", "enable", "recenter", "disable", "body-enable", "body-disable", "status", "finish")]
    [string]$Action = "prepare",

    [string]$GameDirectory = "",

    [switch]$BodyIkAtStart,

    [switch]$StartupOnly,

    [switch]$KeepVideoSettings,

    [switch]$LargeAddressAware,

    [switch]$ReloadWaitProbe,
    [switch]$ManualReload
)

$ErrorActionPreference = "Stop"

$RepositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$LocalStateDirectory = Join-Path $RepositoryRoot "work"
$LocalConfigPath = Join-Path $LocalStateDirectory "vr-test.json"
$VideoProfileStatePath = Join-Path $LocalStateDirectory "vr-video-profile.json"
$VideoProfileBackupPath = Join-Path $LocalStateDirectory "vr-video-profile.backup"
. (Join-Path $PSScriptRoot "coj_video_profile.ps1")

function Get-CoJVideoSettingsPath {
    $Documents = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
    if ([string]::IsNullOrWhiteSpace($Documents)) {
        throw "The Windows Documents folder could not be resolved."
    }
    return Join-Path $Documents "call of juarez\out\Settings\Video.scr"
}

function Restore-VrVideoProfile {
    return Restore-CoJVrVideoProfile -StatePath $VideoProfileStatePath -BackupPath $VideoProfileBackupPath
}

function Apply-VrVideoProfile {
    if (-not (Test-Path -LiteralPath $LocalStateDirectory -PathType Container)) {
        New-Item -ItemType Directory -Path $LocalStateDirectory -Force | Out-Null
    }
    return Apply-CoJVrVideoProfile -VideoPath (Get-CoJVideoSettingsPath) `
        -StatePath $VideoProfileStatePath -BackupPath $VideoProfileBackupPath
}

function Write-LocalConfig([string]$ResolvedGameDirectory) {
    if (-not (Test-Path -LiteralPath $LocalStateDirectory -PathType Container)) {
        New-Item -ItemType Directory -Path $LocalStateDirectory -Force | Out-Null
    }
    $Config = [ordered]@{
        schemaVersion = 1
        gameDirectory = $ResolvedGameDirectory
    }
    [System.IO.File]::WriteAllText(
        $LocalConfigPath,
        ($Config | ConvertTo-Json) + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))
}

function Resolve-GameDirectory {
    if (-not [string]::IsNullOrWhiteSpace($GameDirectory)) {
        $Resolved = [System.IO.Path]::GetFullPath($GameDirectory)
        Write-LocalConfig $Resolved
        return $Resolved
    }
    if (Test-Path -LiteralPath $LocalConfigPath -PathType Leaf) {
        $Config = Get-Content -LiteralPath $LocalConfigPath -Raw | ConvertFrom-Json
        if (-not [string]::IsNullOrWhiteSpace([string]$Config.gameDirectory)) {
            return [System.IO.Path]::GetFullPath([string]$Config.gameDirectory)
        }
    }
    throw "GameDirectory is required the first time. Example: pwsh -File tools\vr_test.ps1 prepare -GameDirectory 'C:\path\to\Call of Juarez'"
}

function Assert-GameClosed {
    if (Get-Process -Name CoJ -ErrorAction SilentlyContinue) {
        throw "Call of Juarez is running. Close it before preparing or finalizing a candidate."
    }
}

function Invoke-Checked([scriptblock]$Command, [string]$FailureMessage) {
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw $FailureMessage
    }
}

$ResolvedGameDirectory = Resolve-GameDirectory
$StageStatePath = Join-Path $ResolvedGameDirectory ".cojvr-d3d9-stage.json"
$CurrentRunPath = Join-Path $ResolvedGameDirectory ".cojvr-run.json"
$ControlPath = Join-Path $ResolvedGameDirectory "cojvr-camera-control.json"
$ExBridgeMarkerPath = Join-Path $ResolvedGameDirectory ".cojvr-d3d9-ex-bridge"

switch ($Action) {
    "prepare" {
        Assert-GameClosed
        if ($BodyIkAtStart -and $StartupOnly) {
            throw "StartupOnly and BodyIkAtStart cannot be combined."
        }
        if ($ReloadWaitProbe -and ($StartupOnly -or -not $BodyIkAtStart)) {
            throw "ReloadWaitProbe requires the full BodyIkAtStart gameplay candidate."
        }
        if ($ManualReload -and ($ReloadWaitProbe -or $StartupOnly -or -not $BodyIkAtStart)) {
            throw 'ManualReload requires full BodyIkAtStart and cannot combine with ReloadWaitProbe.'
        }
        if (Test-Path -LiteralPath $StageStatePath -PathType Leaf) {
            $Existing = Get-Content -LiteralPath $StageStatePath -Raw | ConvertFrom-Json
            throw "A candidate is already staged (run '$($Existing.runId)'). Run 'tools\vr_test.ps1 finish' after closing the game before preparing another one."
        }
        if (Test-Path -LiteralPath $VideoProfileStatePath -PathType Leaf) {
            [void](Restore-VrVideoProfile)
        }

        Push-Location $RepositoryRoot
        try {
            Invoke-Checked { cmake --preset win32-debug } "Win32 configure failed."
            Invoke-Checked { cmake --build --preset release } "Release build failed."
            Invoke-Checked { ctest --preset release } "Release host tests failed."
        } finally {
            Pop-Location
        }

        # Provenance must bind the exact artifact produced by the build/test tree
        # above. Do not stage a similarly named DLL from another build directory.
        $ProxyPath = Join-Path $RepositoryRoot "build\win32-debug\Release\d3d9_native_stereo.dll"
        $ManifestPath = Join-Path $RepositoryRoot "build\win32-debug\Release\d3d9_native_stereo.build-manifest.json"
        & (Join-Path $PSScriptRoot "new_build_manifest.ps1") `
            -RepositoryRoot $RepositoryRoot `
            -Configuration Release `
            -DiagnosticMode d3d9_native_stereo `
            -ProxyPath $ProxyPath `
            -OutputPath $ManifestPath `
            -AllowDirty
        if ($LASTEXITCODE -ne 0) { throw "Build-manifest generation failed." }

        $ValidationProfile = if ($StartupOnly) { "startup" } elseif ($BodyIkAtStart) { "full" } else { "transport" }
        try {
            & (Join-Path $PSScriptRoot "stage_d3d9_proxy.ps1") `
                -GameDirectory $ResolvedGameDirectory `
                -ProxyPath $ProxyPath `
                -BuildManifestPath $ManifestPath `
                -ValidationProfile $ValidationProfile `
                -IndependentHands:$BodyIkAtStart `
                -LargeAddressAware:$LargeAddressAware `
                -ReloadWaitProbe:$ReloadWaitProbe -ManualReload:$ManualReload
            if ($LASTEXITCODE -ne 0) { throw "Native-stereo staging failed." }

            & (Join-Path $PSScriptRoot "set_hmd_camera_control.ps1") `
                -GameDirectory $ResolvedGameDirectory `
                -Mode $(if ($StartupOnly) { "disable" } else { "enable" })

            if ($BodyIkAtStart) {
                & (Join-Path $PSScriptRoot "set_hmd_camera_control.ps1") `
                    -GameDirectory $ResolvedGameDirectory `
                    -Mode body-enable
            }

            $VideoProfile = $null
            if (-not $KeepVideoSettings) {
                $VideoProfile = Apply-VrVideoProfile
                $RunForProfile = Get-Content -LiteralPath $CurrentRunPath -Raw | ConvertFrom-Json
                $RunForProfile.validation | Add-Member -NotePropertyName videoProfile -NotePropertyValue ([ordered]@{
                    resolution = $VideoProfile.resolution
                    fsaa = $VideoProfile.fsaa
                    originalSha256 = $VideoProfile.originalSha256
                    appliedSha256 = $VideoProfile.appliedSha256
                }) -Force
                [System.IO.File]::WriteAllText(
                    $CurrentRunPath,
                    ($RunForProfile | ConvertTo-Json -Depth 10) + [Environment]::NewLine,
                    [System.Text.UTF8Encoding]::new($false))
            }

            # Native stereo owns its D3D9Ex-primary shared-texture transport and
            # a classic CreateDevice fallback internally. Remove the obsolete
            # marker so the generic proxy cannot alter the factory policy.
            if (Test-Path -LiteralPath $ExBridgeMarkerPath -PathType Leaf) {
                Remove-Item -LiteralPath $ExBridgeMarkerPath -Force
            }
            Write-Host "D3D9 transport: integrated D3D9Ex shared-texture primary (classic creation fallback)"
        } catch {
            $PrepareError = $_.Exception.Message
            try { [void](Restore-VrVideoProfile) } catch { $PrepareError += " Video profile rollback failed: $($_.Exception.Message)" }
            if (Test-Path -LiteralPath $StageStatePath -PathType Leaf) {
                try {
                    & (Join-Path $PSScriptRoot "unstage_d3d9_proxy.ps1") -GameDirectory $ResolvedGameDirectory
                } catch {
                    $PrepareError += " Candidate rollback failed: $($_.Exception.Message)"
                }
            }
            throw $PrepareError
        }

        $Run = Get-Content -LiteralPath $CurrentRunPath -Raw | ConvertFrom-Json
        Write-Host ""
        Write-Host "Native-stereo VR test candidate ready."
        Write-Host "Run ID: $($Run.runId)"
        Write-Host "Validation profile: $ValidationProfile"
        Write-Host "Tracking/stereo: $(if ($StartupOnly) { 'disabled for startup diagnosis' } else { 'enabled; first valid HMD pose becomes the base orientation' })."
        Write-Host "Body IK at game start: $($BodyIkAtStart.IsPresent.ToString().ToLowerInvariant())"
        Write-Host "LargeAddressAware executable: $($LargeAddressAware.IsPresent.ToString().ToLowerInvariant()) (original restored by finish)"
        Write-Host "Bounded reload wait/zero-transfer probe: $($ReloadWaitProbe.IsPresent.ToString().ToLowerInvariant()) (gesture route; Square remains native)"
        Write-Host "Persistent manual reload candidate: $($ManualReload.IsPresent.ToString().ToLowerInvariant()) (Square opens/closes; free trigger inserts one native round)"
        Write-Host "VR video profile: $(if ($KeepVideoSettings) { 'unchanged' } else { "$($VideoProfile.resolution), FSAA 0 (selected resolution preserved; restored by finish)" })"
        Write-Host "Start SteamVR manually, then launch Call of Juarez normally."
        Write-Host "Menu pointer: point without L1/R1; same-hand L2/R2 selects and chooses that ray. Mouse motion/drag gets temporary priority. Cross accepts; Circle goes back."
        Write-Host "Gameplay: Options pauses, Cross jumps, Square reloads, Circle toggles alternate fire, L1 performs Action, R1 kicks on foot / holds gallop mounted, L3 toggles crouch and R3 toggles Focus. The sole default table is docs/research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls."
        if ($ManualReload) {
            Write-Host "Manual reload: use a single Peacemaker/Frontier with empty support. Fresh Square prepares; wait for MANUAL_LOAD, pick up with the free trigger at waist, carry to the grip and release. Every accepted fresh release calls native WeaponReload once, without another action31/automatic cycle. Full/no reserve closes; fresh Square or armed trigger closes early. Release before reopening/firing. Integrated acceptance: docs/VALIDATION.md#persistent-manual-reload-candidate."
            Write-Host "Combined candidate batch: inspect finger-derived cartridge axis, soft held-zone cue then stronger accepted-insertion cue, drop/close/fire, held-input recovery and L1 interaction. New changes are host-tested only; compositor still has no hand/scene occlusion. Ordered batch: docs/VALIDATION.md#consolidated-candidate-batch--host-tested-awaiting-runtime-and-visor. Depth diagnostics collect automatically during pickups; no separate physical probe is needed."
        } else {
            Write-Host "Motion reload: with one eligible pistol and the other hand empty, hold the free trigger at waist level, carry it to the pistol and release near the grip. Experimental Peacemaker/Frontier cartridge pacing requests one native round; after the native reload interval, another cartridge appears in the support hand for a fresh trigger press/release near the gun. Square remains available. Legacy paced continuation has bounded live evidence; native animation still owns its hands. Persistent manual reload requires -ManualReload. See docs/VALIDATION.md#cartridge-paced-reload-acceptance."
        }
        Write-Host "Drawer/container Action: point the left Sense at the handle/usable part and press L1. A small cyan hand-directed reference appears only while L1 is held; head gaze no longer selects the object. Native reach/permissions remain. Lost left-hand tracking requires release before another Action. The reference is not proof that a target is usable."
        Write-Host "Death/mission failure: corrected visor visibility has operator acceptance. Circle requests native Back/Escape. Native retry/held-control recovery remains pending; successive manual loading has scoped acceptance."
        Write-Host "Hold Triangle for the equipment wheel: highlight with right stick, release Triangle to confirm once, release in center to cancel. Center right stick after closing before snap resumes. Create tap opens Objectives on release; an 800 ms hold recenters once. Revised available controls and ordinary Focus have operator acceptance; mounted contexts and bow/scoped Winchester retain separate gates."
        Write-Host "Gameplay HUD: native text and visible cached wrist data include health/ammo, posture/shadow and optional concentration/countdown/horse indicators. Check only content available in this session. New lines and empty-hand finger animation remain host-tested; full graphical HUD remains pending."
        Write-Host "The previously inspected NoLogos argument did not bypass the intro videos in physical testing, so it is no longer part of the VR test procedure."
        if ($StartupOnly) {
            Write-Host "Startup-only run: confirm videos and the main menu appear, then close normally. Native stereo/GPU transport are outside this profile; controller menu usability needs operator observations."
        } elseif ($BodyIkAtStart) {
            Write-Host "At the first flat menu, confirm the cyan beam starts at the Sense controller and moving it changes CoJ's highlighted/hovered option."
            Write-Host "UI controls for this gate: the laser is automatic; its L2/R2 selects, Cross accepts, Circle goes back. Exercise all paths and skip at least one startup item with Sense input."
            Write-Host "After loading a save, confirm the press-a-key continuation accepts a Sense action without using the keyboard."
            Write-Host "Release the trigger after ray-select and confirm an 800 ms Create hold re-anchors the flat view without breaking cursor alignment."
            Write-Host "Recenter in-headset: hold Create on the left PS VR2 Sense controller for 800 ms, including once with a small HMD pitch/roll, and confirm the world remains level afterwards. A short tap requests Objectives on release."
            Write-Host "Body/full-profile run: first exercise both controllers/head movement with focus on CoJ; then separately check pause/dashboard recovery, fresh actions and held-button suppression."
            Write-Host "Body IK is already enabled, so no Alt+Tab or terminal toggle is required while the game is running."
            Write-Host "Keep the accepted locomotion baseline and Steam recording off; finish retains GPU transport, frame-age, cadence and producer/consumer wait telemetry."
            Write-Host "Physically crouch by lowering the HMD: first-person ownership should remain stable and the full avatar must not move in front of the camera. Test explicit controller crouch separately."
            Write-Host "Fire one direct shot first and confirm the process remains stable. Then try repeated/held fire where supported and judge origin/direction against the weapon/controller."
            Write-Host "Independent native hands retain torso/legs at native proportions; no T-pose reach calibration is needed. Move hands close to the chest and through full wrist turns, walk/turn and reload."
            if ($ManualReload) {
            Write-Host "In an admitted manual session, verify both hands/weapon follow the controllers through preparation, waiting and coherent closing, with connected arms still hidden. Visible loading-port clearance and insertion sound remain unproven. Unsupported native animations retain their existing presentation."
            } else {
                Write-Host "Confirm detached hands/weapon follow the controllers and original arms return only during native animation."
            }
        } else {
            Write-Host "Transport-profile run: first exercise automatic menu hover, R2/L2 selection, Cross/Circle and physical mouse/drag coexistence. Report accuracy separately; the transport verifier does not accept menu usability."
            Write-Host "Body IK stays off. Slowly turn your head beyond both sides of the 35-degree comfort cone while watching the native revolver hand; report whether the previous body-yaw steps remain."
            Write-Host "Then reach native-stereo gameplay, confirm depth and stable slow/fast head turns, move/walk/jump and recenter with an 800 ms Create hold. Open/close the SteamVR dashboard once and confirm presentation/input resume."
            Write-Host "Body IK remains disabled. Do not judge tracked avatar arms or weapon alignment from this profile. Finish through a normal game quit."
            Write-Host "The acceptance target is stable tracking/stereo with producer cadence following the configured HMD refresh and zero classic-D3D9 fallback/readback."
            Write-Host "Check configured HMD refresh, producer target, rendered pair rate and presenter submission rate separately; the desktop monitor is not the VR cadence target."
        }
        Write-Host "Diagnostic fallback: pwsh -File tools\vr_test.ps1 recenter"
        if ($ValidationProfile -notin @("transport", "startup")) {
            Write-Host "If switching windows is stable, disable before closing to satisfy the live passthrough gate: pwsh -File tools\vr_test.ps1 disable"
            Write-Host "If Alt+Tab stalls/crashes CoJ, do not switch windows just for this command; close normally and finish will preserve the partial evidence and report the missing gate."
        }
        Write-Host "After closing the game: pwsh -File tools\vr_test.ps1 finish"
        break
    }

    { $_ -in @("enable", "recenter", "disable", "body-enable", "body-disable") } {
        & (Join-Path $PSScriptRoot "set_hmd_camera_control.ps1") `
            -GameDirectory $ResolvedGameDirectory `
            -Mode $Action
        break
    }

    "status" {
        $GameRunning = [bool](Get-Process -Name CoJ -ErrorAction SilentlyContinue)
        Write-Host "Game directory: $ResolvedGameDirectory"
        Write-Host "Call of Juarez running: $($GameRunning.ToString().ToLowerInvariant())"
        if (-not (Test-Path -LiteralPath $StageStatePath -PathType Leaf)) {
            Write-Host "Staging: none"
            break
        }
        $Stage = Get-Content -LiteralPath $StageStatePath -Raw | ConvertFrom-Json
        Write-Host "Staging: $($Stage.diagnosticMode)"
        Write-Host "Run ID: $($Stage.runId)"
        if (Test-Path -LiteralPath $ControlPath -PathType Leaf) {
            $Control = Get-Content -LiteralPath $ControlPath -Raw | ConvertFrom-Json
            Write-Host "Tracking enabled: $([bool]$Control.trackingEnabled)"
            Write-Host "Body IK enabled: $([bool]$Control.bodyIkEnabled)"
            Write-Host "Recenter requested: $([bool]$Control.recenter)"
            if ($null -ne $Control.PSObject.Properties['movementTraceEnabled']) {
                Write-Host "Movement trace: $([bool]$Control.movementTraceEnabled) ($([string]$Control.movementTracePhase))"
                Write-Host "VR gameplay input: $([bool]$Control.vrGameplayInputEnabled)"
                Write-Host "Capture/readback: $([bool]$Control.captureReadbackEnabled)"
                Write-Host "Second eye render: $([bool]$Control.secondEyeRenderEnabled)"
            }
        }
        if (Test-Path -LiteralPath $VideoProfileStatePath -PathType Leaf) {
            $VideoState = Get-Content -LiteralPath $VideoProfileStatePath -Raw | ConvertFrom-Json
            Write-Host "VR video profile: $($VideoState.resolution), FSAA $($VideoState.fsaa)"
        } else {
            Write-Host "VR video profile: inactive"
        }
        break
    }

    "finish" {
        Assert-GameClosed
        if (-not (Test-Path -LiteralPath $StageStatePath -PathType Leaf)) {
            $DeploymentJournal = Join-Path $ResolvedGameDirectory ".cojvr-deployment-transaction.json"
            $PendingDeployment = Test-Path -LiteralPath $DeploymentJournal -PathType Leaf
            $PendingVideo = Test-Path -LiteralPath $VideoProfileStatePath -PathType Leaf
            if (-not $PendingDeployment -and -not $PendingVideo) {
                throw "No staged VR test candidate or pending recovery was found."
            }
            # Unstage may have committed its restoration before video/cleanup
            # failed. Retry only retained journals; do not infer a new live pass.
            $RecoveryErrors = @()
            if ($PendingDeployment) {
                try {
                    $RecoveryJournal = Get-Content -LiteralPath $DeploymentJournal -Raw | ConvertFrom-Json
                    if ([string]$RecoveryJournal.operation -notin @('stage', 'unstage') -or
                        @($RecoveryJournal.assets).Count -eq 0 -or
                        @($RecoveryJournal.assets | Where-Object {
                            $_.role -eq 'proxy' -and $_.kind -eq 'file' -and $_.destination -eq 'd3d9.dll'
                        }).Count -ne 1) {
                        throw "Deployment recovery journal has an invalid operation or incomplete asset inventory."
                    }
                    & (Join-Path $PSScriptRoot "unstage_d3d9_proxy.ps1") -GameDirectory $ResolvedGameDirectory
                } catch { $RecoveryErrors += "Deployment recovery failed: $($_.Exception.Message)" }
            }
            try { [void](Restore-VrVideoProfile) } catch {
                $RecoveryErrors += "Video settings recovery failed: $($_.Exception.Message)"
            }
            if ($RecoveryErrors.Count) { throw ($RecoveryErrors -join ' ') }
            Write-Host "Pending VR restoration completed. No live validation was performed during recovery."
            break
        }

        $VerificationError = $null
        try {
            & (Join-Path $PSScriptRoot "verify_native_stereo_live_test.ps1") `
                -GameDirectory $ResolvedGameDirectory
        } catch {
            $VerificationError = $_.Exception.Message
            Write-Warning "Native-stereo verifier failed: $VerificationError"
        }

        $SummaryError = $null
        try {
            & (Join-Path $PSScriptRoot "summarize_native_stereo_run.ps1") `
                -GameDirectory $ResolvedGameDirectory
            & (Join-Path $PSScriptRoot "summarize_movement_diagnostic.ps1") `
                -GameDirectory $ResolvedGameDirectory
        } catch {
            $SummaryError = $_.Exception.Message
            Write-Warning "Native-stereo summary failed: $SummaryError"
        }

        $CollectionError = $null
        try {
            & (Join-Path $PSScriptRoot "collect_run_evidence.ps1") `
                -GameDirectory $ResolvedGameDirectory
        } catch {
            $CollectionError = $_.Exception.Message
            Write-Warning "Evidence collection failed: $CollectionError"
        }

        $UnstageError = $null
        try {
            & (Join-Path $PSScriptRoot "unstage_d3d9_proxy.ps1") `
                -GameDirectory $ResolvedGameDirectory
        } catch {
            $UnstageError = $_.Exception.Message
            Write-Warning "Candidate restore failed: $UnstageError"
        }

        # Remove D3D9Ex bridge marker file
        if (Test-Path -LiteralPath $ExBridgeMarkerPath -PathType Leaf) {
            Remove-Item -LiteralPath $ExBridgeMarkerPath -Force -ErrorAction SilentlyContinue
        }

        $VideoRestoreError = $null
        try {
            [void](Restore-VrVideoProfile)
        } catch {
            $VideoRestoreError = $_.Exception.Message
            Write-Warning "Video settings restore failed: $VideoRestoreError"
        }

        $SummaryPath = Join-Path $ResolvedGameDirectory "cojvr-native-stereo-summary.json"
        if (Test-Path -LiteralPath $SummaryPath -PathType Leaf) {
            Remove-Item -LiteralPath $SummaryPath -Force
        }
        $MovementSummaryPath = Join-Path $ResolvedGameDirectory "cojvr-movement-summary.json"
        if (Test-Path -LiteralPath $MovementSummaryPath -PathType Leaf) {
            Remove-Item -LiteralPath $MovementSummaryPath -Force
        }

        if ($CollectionError) {
            throw "Candidate was unstaged, but evidence collection failed: $CollectionError"
        }
        if ($UnstageError) {
            throw "Evidence was processed, but the staged candidate could not be restored: $UnstageError"
        }
        if ($VideoRestoreError) {
            throw "Candidate staging was restored, but Video.scr could not be restored: $VideoRestoreError"
        }
        if ($SummaryError) {
            throw "Candidate evidence was collected and the game directory was restored, but the performance/state summary failed: $SummaryError"
        }
        if ($VerificationError) {
            throw "Candidate evidence was collected and the game directory was restored, but the live verifier failed: $VerificationError"
        }
        Write-Host "VR test finalized: evidence verified/collected and staging restored."
        break
    }
}
