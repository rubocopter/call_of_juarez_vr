param(
    [ValidateSet("prepare", "enable", "recenter", "disable", "body-enable", "body-disable", "status", "finish")]
    [string]$Action = "prepare",

    [string]$GameDirectory = "",

    [switch]$BodyIkAtStart,

    [switch]$StartupOnly,

    [switch]$KeepVideoSettings,

    [switch]$LargeAddressAware,

    [switch]$ReloadWaitProbe,
    [switch]$ManualReload,
    [switch]$RenderResourceProbe,
    [switch]$FrameLumaProbe,
    [switch]$MenuPresentationCheck,
    [ValidateSet('off','2560x1440','2560x1920')]
    [string]$IndependentEyeResolution = 'off',
    [ValidateSet('preserve','2804x2860')]
    [string]$NativeResolution = 'preserve',
    [ValidateSet('Preserve','Auto','75%','100%','125%','Manual')]
    [string]$ResolutionProfile = 'Preserve',
    [object]$ResolutionWidth = 0,
    [object]$ResolutionHeight = 0,
    [switch]$ExperimentalResolutionProfile,
    [switch]$UseSavedResolutionProfile,
    [string]$ResolutionSettingsPath = ''
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
    param([ValidateSet('preserve','2804x2860')] [string]$NativeResolution = 'preserve',
          [object]$ExperimentalWidth=0,[object]$ExperimentalHeight=0)
    if (-not (Test-Path -LiteralPath $LocalStateDirectory -PathType Container)) {
        New-Item -ItemType Directory -Path $LocalStateDirectory -Force | Out-Null
    }
    return Apply-CoJVrVideoProfile -VideoPath (Get-CoJVideoSettingsPath) `
        -StatePath $VideoProfileStatePath -BackupPath $VideoProfileBackupPath -NativeResolution $NativeResolution `
        -ExperimentalWidth $ExperimentalWidth -ExperimentalHeight $ExperimentalHeight
}

function Invoke-VrResolutionObservation {
    $PreparationId='resolution-'+[guid]::NewGuid().ToString('N')
    $Helper=Join-Path $RepositoryRoot 'build/win32-debug/Release/cojvr_openvr_resolution_query.exe'
    $Raw=& $Helper --preparation-id $PreparationId
    $ExitCode=$LASTEXITCODE
    if ($ExitCode -notin @(0,2)) {throw 'Resolution observation helper failed.'}
    try {$Record=($Raw -join "`n") | ConvertFrom-Json} catch {throw 'Resolution helper did not return a valid observation.'}
    return [pscustomobject]@{PreparationId=$PreparationId;Record=$Record}
}

function Invoke-VrNativeResolutionLimits {
    param([int]$Width,[int]$Height)
    $Helper=Join-Path $RepositoryRoot 'build/win32-debug/Release/cojvr_d3d9_resolution_limits.exe'
    $Raw=& $Helper --width $Width --height $Height
    $ExitCode=$LASTEXITCODE
    try {$Record=($Raw -join "`n") | ConvertFrom-Json} catch {throw 'Invalid native resolution limits report.'}
    if ($ExitCode -notin @(0,2) -or $Record.schemaVersion -ne 1 -or
        $Record.recordType -ne 'cojvr-native-resolution-limits' -or $Record.adapterIndex -ne 0 -or
        $Record.requestedWidth -ne $Width -or $Record.requestedHeight -ne $Height -or
        $Record.status -notin @('caps_compatible','incompatible','unavailable') -or
        (($ExitCode -eq 0) -ne ($Record.status -eq 'caps_compatible'))) {
        throw 'Native resolution limits report is inconsistent.'
    }
    return $Record
}

function Resolve-VrResolutionProfile {
    param([ValidateSet('Preserve','Auto','75%','100%','125%','Manual')][string]$Profile='Preserve',
          [object]$Width=0,[object]$Height=0,[switch]$Experimental)
    $Width=ConvertTo-CoJNativeDimension $Width
    $Height=ConvertTo-CoJNativeDimension $Height
    if ($Profile -eq 'Preserve') {
        if ($Width -or $Height -or $Experimental) {throw 'Explicit dimensions/experimental opt-in require a resolution profile.'}
        return [pscustomobject]@{Status='preserve';Width=0;Height=0;Experimental=$false;Profile=$Profile;Proposal=$null}
    }
    if (-not $Experimental) {throw 'Resolution profiles require ExperimentalResolutionProfile until release qualification is complete.'}
    $Resolver=Join-Path $RepositoryRoot 'tools/coj_resolution_profiles.py'
    if ($Profile -eq 'Manual') {
        $Raw=python $Resolver --profile Manual --width $Width --height $Height
    } else {
        if ($Width -or $Height) {throw 'Only Manual accepts explicit dimensions.'}
        $Observation=Invoke-VrResolutionObservation
        $Raw=$Observation.Record | ConvertTo-Json -Depth 6 -Compress |
            python $Resolver --profile $Profile --observation - --preparation-id $Observation.PreparationId
    }
    $ExitCode=$LASTEXITCODE
    try {$Proposal=($Raw -join "`n") | ConvertFrom-Json} catch {throw 'Invalid resolution proposal report.'}
    if ($ExitCode -ne 0 -or $Proposal.status -eq 'rejected') {throw ('Resolution request rejected: '+($Proposal.errors -join '; '))}
    if ($Proposal.status -eq 'unavailable') {
        return [pscustomobject]@{Status='preserve_unavailable';Width=0;Height=0;Experimental=$false;Profile=$Profile;Proposal=$Proposal}
    }
    if ($Proposal.status -ne 'proposal_only' -or @($Proposal.candidateDimensions).Count -ne 2 -or $Proposal.applyAllowed) {
        throw 'Unexpected resolution proposal admission state.'
    }
    # This bounded trial checks only the native default adapter. DXGI and D3D9
    # ordinals are not an adapter-identity proof; multi-GPU remains unqualified.
    if ($Profile -ne 'Manual' -and $Observation.Record.adapterIndex -ne 0) {
        return [pscustomobject]@{Status='preserve_adapter_unqualified';Width=0;Height=0;
            Experimental=$false;Profile=$Profile;Proposal=$Proposal}
    }
    $Limits=Invoke-VrNativeResolutionLimits -Width $Proposal.candidateDimensions[0] -Height $Proposal.candidateDimensions[1]
    if ($Limits.status -ne 'caps_compatible') {
        return [pscustomobject]@{Status='preserve_incompatible';Width=0;Height=0;Experimental=$false;
            Profile=$Profile;Proposal=$Proposal;Limits=$Limits}
    }
    # Explicit controlled native-video trial, not public automatic admission.
    # Native allocation, x86 memory and actual raster quality remain physical gates.
    return [pscustomobject]@{Status='experimental_trial';Width=[int]$Proposal.candidateDimensions[0];
        Height=[int]$Proposal.candidateDimensions[1];Experimental=$true;Profile=$Profile;Proposal=$Proposal;Limits=$Limits}
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
        if ($UseSavedResolutionProfile) {
            if ($PSBoundParameters.ContainsKey('ResolutionProfile') -or $ResolutionWidth -or $ResolutionHeight -or
                $ExperimentalResolutionProfile -or $NativeResolution -ne 'preserve') {
                throw 'Saved resolution selection cannot be combined with an explicit override.'
            }
            . (Join-Path $PSScriptRoot 'coj_resolution_settings.ps1')
            . (Join-Path $PSScriptRoot 'coj_resolution_session.ps1')
            if ([string]::IsNullOrWhiteSpace($ResolutionSettingsPath)) {$ResolutionSettingsPath=Get-CoJResolutionSettingsPath}
            $SavedResolution=Read-CoJResolutionSettings -Path $ResolutionSettingsPath
            $ResolutionProfile=$SavedResolution.profile
            $ResolutionWidth=$SavedResolution.width;$ResolutionHeight=$SavedResolution.height
            $ExperimentalResolutionProfile=[switch]($ResolutionProfile -ne 'Preserve')
        }
        Assert-GameClosed
        $ResolutionWidth=ConvertTo-CoJNativeDimension $ResolutionWidth
        $ResolutionHeight=ConvertTo-CoJNativeDimension $ResolutionHeight
        $ProfileRequested=$ResolutionProfile -ne 'Preserve'
        if (($ResolutionWidth -or $ResolutionHeight -or $ExperimentalResolutionProfile) -and -not $ProfileRequested) {
            throw 'Explicit dimensions/experimental opt-in require ResolutionProfile.'
        }
        if ($ProfileRequested -and ($NativeResolution -ne 'preserve' -or -not $ExperimentalResolutionProfile -or
                $KeepVideoSettings -or $StartupOnly -or -not $BodyIkAtStart -or -not $LargeAddressAware -or $IndependentEyeResolution -ne 'off')) {
            throw 'Experimental profiles require full LAA BodyIkAtStart, reversible video, bank off, and no legacy NativeResolution override.'
        }
        if ($ResolutionProfile -ne 'Manual' -and ($ResolutionWidth -or $ResolutionHeight)) {throw 'Only Manual accepts explicit dimensions.'}
        if ($MenuPresentationCheck -and $NativeResolution -ne '2804x2860' -and
                -not ($ResolutionProfile -eq 'Manual' -and $ResolutionWidth -eq 2804 -and $ResolutionHeight -eq 2860)) {
            throw 'MenuPresentationCheck requires the explicit 2804x2860 native trial.'
        }
        if ($NativeResolution -ne 'preserve' -or $ProfileRequested) {
            if ($KeepVideoSettings -or $StartupOnly -or -not $BodyIkAtStart -or -not $LargeAddressAware -or $IndependentEyeResolution -ne 'off') {
                throw 'NativeResolution requires full BodyIkAtStart, LargeAddressAware, video-profile restoration and independent resolution off.'
            }
            # Dimension qualification uses the binding census. The flash probe
            # observes pixels instead and keeps the expensive census off.
            if (-not $FrameLumaProbe -and -not $MenuPresentationCheck) { $RenderResourceProbe = [switch]$true }
        }
        if ($FrameLumaProbe -and ($StartupOnly -or -not $BodyIkAtStart -or $IndependentEyeResolution -ne 'off')) {
            throw 'FrameLumaProbe requires ordinary native stereo with BodyIkAtStart and independent resolution off.'
        }
        if ($IndependentEyeResolution -ne 'off' -and ($StartupOnly -or -not $BodyIkAtStart -or -not $LargeAddressAware)) {
            throw 'IndependentEyeResolution requires full BodyIkAtStart and LargeAddressAware.'
        }
        if ($RenderResourceProbe -and ($StartupOnly -or -not $BodyIkAtStart)) {
            throw "RenderResourceProbe requires the full BodyIkAtStart candidate."
        }
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
            if ($MenuPresentationCheck -or $FrameLumaProbe) {
                Invoke-Checked { cmake --build --preset release --target cojvr_d3d9_native_stereo_proxy cojvr_vr_math_test cojvr_ui_cursor_jni_test cojvr_flat_menu_composition_test cojvr_frame_mailbox_test cojvr_body_adapter_test cojvr_camera_probe_test cojvr_independent_hand_test } "Focused candidate build failed."
                Invoke-Checked { ctest --preset release -R '^(vr_math|ui_cursor_jni|flat_menu_composition|frame_mailbox|player_mesh_split|body_adapter|camera_probe|independent_hand|vr_video_profile|native_resolution_analysis|render_resource_analysis)$' } "Focused host tests failed."
                if ($FrameLumaProbe) {
                    Invoke-Checked { cmake --build --preset release --target cojvr_frame_luma_probe_test cojvr_camera_probe_test } "Luma probe build failed."
                    Invoke-Checked { ctest --preset release -R '^(frame_luma_probe|frame_luma_probe_ex|camera_probe|render_resource_controls|frame_luma_analysis)$' } "Luma probe host tests failed."
                }
            } else {
                Invoke-Checked { cmake --build --preset release } "Release build failed."
                Invoke-Checked { ctest --preset release } "Release host tests failed."
            }
            if ($ProfileRequested) {
                Invoke-Checked { cmake --build --preset release --target cojvr_openvr_resolution_query cojvr_d3d9_resolution_limits } "Resolution helper build failed."
                Invoke-Checked { ctest --preset release -R '^(resolution_profiles|resolution_profile_workflow|openvr_resolution_query_cli|d3d9_resolution_limits_cli)$' } "Resolution profile host tests failed."
            }
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

        $ResolutionDecision=if ($UseSavedResolutionProfile) {
            Resolve-CoJLaunchResolution -Settings $SavedResolution -PythonExecutable 'python' `
                -ResolverPath (Join-Path $PSScriptRoot 'coj_resolution_profiles.py') `
                -QueryHelperPath (Join-Path $RepositoryRoot 'build/win32-debug/Release/cojvr_openvr_resolution_query.exe') `
                -LimitsHelperPath (Join-Path $RepositoryRoot 'build/win32-debug/Release/cojvr_d3d9_resolution_limits.exe')
        } else {Resolve-VrResolutionProfile -Profile $ResolutionProfile -Width $ResolutionWidth -Height $ResolutionHeight -Experimental:$ExperimentalResolutionProfile}
        $ValidationProfile = if ($StartupOnly) { "startup" } elseif ($BodyIkAtStart) { "full" } else { "transport" }
        try {
            & (Join-Path $PSScriptRoot "stage_d3d9_proxy.ps1") `
                -GameDirectory $ResolvedGameDirectory `
                -ProxyPath $ProxyPath `
                -BuildManifestPath $ManifestPath `
                -ValidationProfile $ValidationProfile `
                -IndependentHands:$BodyIkAtStart `
                -LargeAddressAware:$LargeAddressAware `
                -ReloadWaitProbe:$ReloadWaitProbe -ManualReload:$ManualReload -RenderResourceProbe:$RenderResourceProbe -FrameLumaProbe:$FrameLumaProbe `
                -IndependentEyeResolution $IndependentEyeResolution
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
                if ($ResolutionDecision.Experimental -and $ResolutionProfile -ne 'Manual') {
                    $ObservationAge=([DateTimeOffset]::UtcNow-[DateTimeOffset]::Parse($ResolutionDecision.Proposal.observation.observedUtc)).TotalSeconds
                    if ($ObservationAge -lt 0 -or $ObservationAge -gt 120) {throw 'Resolution observation expired before video apply; prepare a fresh session.'}
                }
                $VideoProfile = Apply-VrVideoProfile -NativeResolution $NativeResolution `
                    -ExperimentalWidth $ResolutionDecision.Width -ExperimentalHeight $ResolutionDecision.Height
                $RunForProfile = Get-Content -LiteralPath $CurrentRunPath -Raw | ConvertFrom-Json
                if ($MenuPresentationCheck) {
                    $RunForProfile.validation | Add-Member -NotePropertyName operatorScope -NotePropertyValue 'main_menu_presentation' -Force
                    $RunForProfile.validation | Add-Member -NotePropertyName hostTestScope -NotePropertyValue 'vr_math,ui_cursor_jni,flat_menu_composition,frame_mailbox,player_mesh_split,body_adapter,camera_probe,independent_hand,vr_video_profile,native_resolution_analysis,render_resource_analysis' -Force
                }
                $RunForProfile.validation | Add-Member -NotePropertyName videoProfile -NotePropertyValue ([ordered]@{
                    resolution = $VideoProfile.resolution
                    nativeResolution = $VideoProfile.nativeResolution
                    originalResolution = $VideoProfile.originalResolution
                    windowed = $VideoProfile.windowed
                    fsaa = $VideoProfile.fsaa
                    originalSha256 = $VideoProfile.originalSha256
                    appliedSha256 = $VideoProfile.appliedSha256
                    resolutionProfile = $ResolutionProfile
                    experimentalResolution = $ResolutionDecision.Experimental
                    resolutionDecision = $ResolutionDecision
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
        if ($ProfileRequested) {
            Write-Host "Startup resolution profile: $ResolutionProfile; decision=$($ResolutionDecision.Status); actual requested native size=$($VideoProfile.resolution)."
            Write-Host 'Experimental application is separate from release qualification; native memory, per-eye raster size and headset performance remain unqualified at new dimensions.'
        }
        if ($FrameLumaProbe) {
            Write-Host 'White-flash diagnostic: fixed 16x16 samples of each native eye and desktop before Present; no image correction, effect edits or resolution transitions.'
            Write-Host 'Start manually; observe the main menu and a short gameplay segment until a flash occurs, then close normally and run finish.'
            Write-Host 'Pixel sampling/readback perturbs timing; this run diagnoses content, not performance acceptance.'
            break
        }
        if ($MenuPresentationCheck) {
            Write-Host 'Focused menu presentation candidate: native 2804x2860; independent bank off; original video restored by finish.'
            Write-Host 'Only the existing visible global MenuMain receives the aspect-preserving reference rectangle; output allocations remain source-owned.'
            Write-Host 'Start SteamVR/game manually. Check main menu size, ray/hover/L2/R2 selection and mouse priority; enter gameplay; open/close pause; return to main menu.'
            Write-Host 'No campaign segment or naturally unavailable cinematic is required. New menu comfort/navigation remain unvalidated until this check.'
            Write-Host 'The canonical full-profile verifier retains separate requirements; unexercised gates are not promoted by this focused operator scope.'
            Write-Host 'Close normally, then run: pwsh -File tools/vr_test.ps1 finish'
            break
        }
        Write-Host "Validation profile: $ValidationProfile"
        Write-Host "Tracking/stereo: $(if ($StartupOnly) { 'disabled for startup diagnosis' } else { 'enabled; first valid HMD pose becomes the base orientation' })."
        Write-Host "Read-only render resource probe: $($RenderResourceProbe.IsPresent). Diagnostic overhead excludes performance qualification."
        Write-Host "Independent eye prototype: $IndependentEyeResolution (startup only; desktop dimensions preserved; unsupported context falls back to native theater)."
        Write-Host "Body IK at game start: $($BodyIkAtStart.IsPresent.ToString().ToLowerInvariant())"
        Write-Host "LargeAddressAware executable: $($LargeAddressAware.IsPresent.ToString().ToLowerInvariant()) (original restored by finish)"
        Write-Host "Bounded reload wait/zero-transfer probe: $($ReloadWaitProbe.IsPresent.ToString().ToLowerInvariant()) (gesture route; Square remains native)"
        Write-Host "Persistent manual reload candidate: $($ManualReload.IsPresent.ToString().ToLowerInvariant()) (Square opens/closes; free trigger inserts one native round)"
        Write-Host "VR video profile: $(if ($KeepVideoSettings) { 'unchanged' } else { "$($VideoProfile.resolution), FSAA 0, native override=$NativeResolution (restored by finish)" })"
        Write-Host "Start SteamVR manually, then launch Call of Juarez normally."
        Write-Host "Menu pointer: point without L1/R1; same-hand L2/R2 selects and chooses that ray. Mouse motion/drag gets temporary priority. Cross accepts; Circle goes back."
        Write-Host "Gameplay: Options pauses, Cross jumps, Square reloads, Circle toggles alternate fire, L1 performs Action, R1 kicks on foot / holds gallop mounted, L3 toggles crouch and R3 toggles Focus. The sole default table is docs/research/COJ_PC_CONTROLS_AND_HUD.md#default-sense-controls."
        if ($ManualReload) {
            Write-Host "Manual reload: use one admitted Peacemaker, Frontier, Walker or Cogswell with empty support. Additional profiles are host-tested; Schofield retains native whole-clip reload. Fresh Square prepares; wait for MANUAL_LOAD, pick up with the free trigger at waist, carry to the grip and release. Every accepted fresh release calls native WeaponReload once, without another action31/automatic cycle. Full/no reserve closes; fresh Square or armed trigger closes early. Release before reopening/firing. Integrated acceptance: docs/VALIDATION.md#persistent-manual-reload-candidate."
            Write-Host "Combined candidate batch: inspect finger-derived cartridge axis, native-confirmed thumb gesture, soft held-zone cue then stronger accepted-insertion cue, drop/close/fire, held-input recovery and L1 interaction. New changes are host-tested only; compositor still has no hand/scene occlusion. Ordered batch: docs/VALIDATION.md#consolidated-candidate-batch--host-tested-awaiting-runtime-and-visor. Depth diagnostics collect automatically during pickups; no separate physical probe is needed."
        } else {
            Write-Host "Motion reload: with one eligible pistol and the other hand empty, hold the free trigger at waist level, carry it to the pistol and release near the grip. Experimental exact-profile cartridge pacing requests one native round; after the native reload interval, another cartridge appears in the support hand for a fresh trigger press/release near the gun. Square remains available. Legacy paced continuation has bounded live evidence; native animation still owns its hands. Persistent manual reload requires -ManualReload. See docs/VALIDATION.md#cartridge-paced-reload-acceptance."
        }
        Write-Host "Drawer/container Action: point the left Sense at the handle/usable part and press L1. A small cyan hand-directed reference appears only while L1 is held; head gaze no longer selects the object. Native reach/permissions remain. Lost left-hand tracking requires release before another Action. The reference is not proof that a target is usable."
        Write-Host "Death/mission failure: corrected visor visibility has operator acceptance. Circle requests native Back/Escape. Native retry/held-control recovery remains pending; successive manual loading has scoped acceptance."
        Write-Host "Hold Triangle for the equipment wheel: highlight with right stick, release Triangle to confirm once, release in center to cancel. Center right stick after closing before smooth turning resumes. During a prepared duel, center then pull right stick down to draw, up to raise, left/right to aim; native timing and triggers remain authoritative. Clyde draw/raise and tracked aim/fire have operator acceptance; broader duels/recovery and smooth-turn comfort remain pending. Create tap opens Objectives on release; an 800 ms hold recenters once. Revised available controls and ordinary Focus have operator acceptance; mounted contexts and bow/scoped Winchester retain separate gates."
        Write-Host "Long-gun support: while a normally held right rifle owns its verified visual map, its logically empty left hand may use controller finger curls. The exercised normal-state correction has operator headset acceptance; broader transitions remain separate. Fire/reload/alternate states retain native fingers. Square retains automatic native long-gun reload and its original animation; physical manual cartridge loading is limited to admitted revolvers."
        Write-Host "Sense tutorial: Cross dismisses a blocking hint; release, then Circle requests Z/alternate fire in gameplay. R3 is Focus/zoom. Ray concentration needs both pistols holstered and native readiness/cooldown: with both drawn, Triangle + right wheel sector/release hides right pistol; center, Triangle + left/release hides left pistol. Let both animations finish, then freshly press L2/R2. Down/Hands is fists, not empty hands."
        Write-Host "VR labels follow startup Game.ini Locale En/Es; unknown settings use English. Native copied text keeps its own localization. Blocking native tutorial images still contain PC keys. Source UTF-8 and UTF-16 accent preservation are host-tested; wording and visor fit join the combined session."
        Write-Host "Gameplay HUD: native text and visible cached wrist data include health/ammo, posture/shadow and optional concentration/countdown/horse indicators. Concentration state glyphs and paired horse health/fatigue meters are host-tested; check them only when naturally available in the combined session. Fatigue is not remaining stamina. Full native graphical HUD capture remains pending."
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
        $DeploymentJournal = Join-Path $ResolvedGameDirectory ".cojvr-deployment-transaction.json"
        $PendingDeployment = Test-Path -LiteralPath $DeploymentJournal -PathType Leaf
        $RestorationPending = $false
        if ($PendingDeployment) {
            $PendingJournal = Get-Content -LiteralPath $DeploymentJournal -Raw | ConvertFrom-Json
            $RestorationPending = [string]$PendingJournal.operation -eq 'unstage'
        }
        if ($RestorationPending -or -not (Test-Path -LiteralPath $StageStatePath -PathType Leaf)) {
            $PendingVideo = Test-Path -LiteralPath $VideoProfileStatePath -PathType Leaf
            if (-not $PendingDeployment -and -not $PendingVideo) {
                throw "No staged VR test candidate or pending recovery was found."
            }
            # Unstage can fail before removing stage state, after retiring live
            # inputs. Retry only restoration; preserve the original verdict/ZIP
            # instead of verifying or collecting partially restored files.
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
