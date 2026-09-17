param(
    [ValidateSet("prepare", "enable", "recenter", "disable", "body-enable", "body-disable", "status", "finish")]
    [string]$Action = "prepare",

    [string]$GameDirectory = "",

    [switch]$BodyIkAtStart,

    [switch]$KeepVideoSettings
)

$ErrorActionPreference = "Stop"

$RepositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$LocalStateDirectory = Join-Path $RepositoryRoot "work"
$LocalConfigPath = Join-Path $LocalStateDirectory "vr-test.json"
$VideoProfileStatePath = Join-Path $LocalStateDirectory "vr-video-profile.json"
$VideoProfileBackupPath = Join-Path $LocalStateDirectory "vr-video-profile.backup"

function Get-CoJVideoSettingsPath {
    $Documents = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
    if ([string]::IsNullOrWhiteSpace($Documents)) {
        throw "The Windows Documents folder could not be resolved."
    }
    return Join-Path $Documents "call of juarez\out\Settings\Video.scr"
}

function Restore-VrVideoProfile {
    if (-not (Test-Path -LiteralPath $VideoProfileStatePath -PathType Leaf)) {
        return $false
    }
    $State = Get-Content -LiteralPath $VideoProfileStatePath -Raw | ConvertFrom-Json
    $VideoPath = [string]$State.videoPath
    if (-not (Test-Path -LiteralPath $VideoProfileBackupPath -PathType Leaf)) {
        throw "The VR video-profile backup is missing; refusing to overwrite '$VideoPath'."
    }
    $BackupHash = (Get-FileHash -LiteralPath $VideoProfileBackupPath -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($BackupHash -ne ([string]$State.originalSha256).ToUpperInvariant()) {
        throw "The VR video-profile backup hash does not match the recorded original settings."
    }
    Copy-Item -LiteralPath $VideoProfileBackupPath -Destination $VideoPath -Force
    $RestoredHash = (Get-FileHash -LiteralPath $VideoPath -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($RestoredHash -ne $BackupHash) {
        throw "Video.scr could not be restored byte-for-byte from the recorded backup."
    }
    Remove-Item -LiteralPath $VideoProfileBackupPath -Force
    Remove-Item -LiteralPath $VideoProfileStatePath -Force
    Write-Host "Restored the original Call of Juarez Video.scr settings."
    return $true
}

function Apply-VrVideoProfile {
    if (Test-Path -LiteralPath $VideoProfileStatePath -PathType Leaf) {
        throw "A VR video profile is already active. Finalize or restore it before preparing another candidate."
    }
    if (-not (Test-Path -LiteralPath $LocalStateDirectory -PathType Container)) {
        New-Item -ItemType Directory -Path $LocalStateDirectory -Force | Out-Null
    }
    $VideoPath = Get-CoJVideoSettingsPath
    if (-not (Test-Path -LiteralPath $VideoPath -PathType Leaf)) {
        throw "Call of Juarez Video.scr was not found at '$VideoPath'."
    }

    Copy-Item -LiteralPath $VideoPath -Destination $VideoProfileBackupPath -Force
    $OriginalHash = (Get-FileHash -LiteralPath $VideoProfileBackupPath -Algorithm SHA256).Hash.ToUpperInvariant()
    try {
        $Text = [System.IO.File]::ReadAllText($VideoPath)
        $ResolutionPattern = '(?m)^Resolution\(\s*\d+\s*,\s*\d+\s*\)[^\S\r\n]*(?=\r?$)'
        $FsaaPattern = '(?m)^FSAA\(\s*\d+\s*\)[^\S\r\n]*(?=\r?$)'
        if ([regex]::Matches($Text, $ResolutionPattern).Count -ne 1 -or
            [regex]::Matches($Text, $FsaaPattern).Count -ne 1) {
            throw "Video.scr does not contain exactly one Resolution(...) and FSAA(...) setting."
        }
        $Updated = [regex]::Replace($Text, $ResolutionPattern, 'Resolution(1920,1080)')
        $Updated = [regex]::Replace($Updated, $FsaaPattern, 'FSAA(0)')
        [System.IO.File]::WriteAllText(
            $VideoPath,
            $Updated,
            [System.Text.UTF8Encoding]::new($false))
        $AppliedHash = (Get-FileHash -LiteralPath $VideoPath -Algorithm SHA256).Hash.ToUpperInvariant()
        $State = [ordered]@{
            schemaVersion = 1
            videoPath = $VideoPath
            originalSha256 = $OriginalHash
            appliedSha256 = $AppliedHash
            resolution = "1920x1080"
            fsaa = 0
        }
        [System.IO.File]::WriteAllText(
            $VideoProfileStatePath,
            ($State | ConvertTo-Json) + [Environment]::NewLine,
            [System.Text.UTF8Encoding]::new($false))
        Write-Host "Applied reversible VR video profile: 1920x1080, FSAA 0."
        return [pscustomobject]$State
    } catch {
        Copy-Item -LiteralPath $VideoProfileBackupPath -Destination $VideoPath -Force
        Remove-Item -LiteralPath $VideoProfileBackupPath -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $VideoProfileStatePath -Force -ErrorAction SilentlyContinue
        throw
    }
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

switch ($Action) {
    "prepare" {
        Assert-GameClosed
        if (Test-Path -LiteralPath $StageStatePath -PathType Leaf) {
            $Existing = Get-Content -LiteralPath $StageStatePath -Raw | ConvertFrom-Json
            throw "A candidate is already staged (run '$($Existing.runId)'). Run 'tools\vr_test.ps1 finish' after closing the game before preparing another one."
        }
        if (Test-Path -LiteralPath $VideoProfileStatePath -PathType Leaf) {
            [void](Restore-VrVideoProfile)
        }

        Push-Location $RepositoryRoot
        try {
            Invoke-Checked { cmake --build --preset release } "Release build failed."
            Invoke-Checked { ctest --preset release } "Release host tests failed."
        } finally {
            Pop-Location
        }

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

        $ValidationProfile = if ($BodyIkAtStart) { "full" } else { "performance" }
        try {
            & (Join-Path $PSScriptRoot "stage_d3d9_proxy.ps1") `
                -GameDirectory $ResolvedGameDirectory `
                -ProxyPath $ProxyPath `
                -BuildManifestPath $ManifestPath `
                -ValidationProfile $ValidationProfile
            if ($LASTEXITCODE -ne 0) { throw "Native-stereo staging failed." }

            & (Join-Path $PSScriptRoot "set_hmd_camera_control.ps1") `
                -GameDirectory $ResolvedGameDirectory `
                -Mode enable

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
        Write-Host "Tracking/stereo: enabled; first valid HMD pose becomes the base orientation."
        Write-Host "Body IK at game start: $($BodyIkAtStart.IsPresent.ToString().ToLowerInvariant())"
        Write-Host "VR video profile: $(if ($KeepVideoSettings) { 'unchanged' } else { '1920x1080, FSAA 0 (restored by finish)' })"
        Write-Host "Start SteamVR manually, then launch Call of Juarez normally."
        Write-Host "The previously inspected NoLogos argument did not bypass the intro videos in physical testing, so it is no longer part of the VR test procedure."
        Write-Host "Recenter in-headset: press Create on the left PS VR2 Sense controller."
        Write-Host "During stable gameplay, open the SteamVR dashboard once, leave it visible briefly, then close it and confirm VR presentation resumes."
        Write-Host "For the performance gate, include slow head turns, fast head turns and mouse rotation after the dashboard cycle."
        if ($BodyIkAtStart) {
            Write-Host "This run also requires the experimental arm/body gate."
        } else {
            Write-Host "Body IK/positional-6DOF promotion is parked for this performance run while campaign actor discovery remains unresolved."
        }
        Write-Host "Diagnostic fallback: pwsh -File tools\vr_test.ps1 recenter"
        Write-Host "If switching windows is stable, disable before closing to satisfy the live passthrough gate: pwsh -File tools\vr_test.ps1 disable"
        Write-Host "If Alt+Tab stalls/crashes CoJ, do not switch windows just for this command; close normally and finish will preserve the partial evidence and report the missing gate."
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
            throw "No staged VR test candidate was found."
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
