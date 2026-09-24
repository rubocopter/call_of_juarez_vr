param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDirectory,

    [Parameter(Mandatory = $true)]
    [string]$BinaryDirectory
)

$ErrorActionPreference = "Stop"

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) {
        throw $Message
    }
}

function Write-Utf8Json([string]$Path, [object]$Value) {
    [System.IO.File]::WriteAllText(
        $Path,
        ($Value | ConvertTo-Json -Depth 10) + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))
}

$SourceDirectory = [System.IO.Path]::GetFullPath($SourceDirectory)
$BinaryDirectory = [System.IO.Path]::GetFullPath($BinaryDirectory)
$TestRoot = Join-Path $BinaryDirectory ("provenance-test-" + [Guid]::NewGuid().ToString("N"))
$ResolvedBinaryDirectory = $BinaryDirectory.TrimEnd("\") + "\"
$ResolvedTestRoot = [System.IO.Path]::GetFullPath($TestRoot)
if (-not $ResolvedTestRoot.StartsWith($ResolvedBinaryDirectory, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to use a test directory outside the configured binary directory."
}

try {
    New-Item -ItemType Directory -Path $TestRoot -Force | Out-Null
    $Scripts = @(
        "new_build_manifest.ps1",
        "collect_run_evidence.ps1",
        "get_run_provenance.ps1",
        "deployment_transaction.ps1",
        "test_deployment_transactions.ps1",
        "stage_d3d9_proxy.ps1",
        "unstage_d3d9_proxy.ps1",
        "stage_d3d9_openvr_flat.ps1",
        "set_camera_probe_control.ps1",
        "verify_camera_probe_live_test.ps1",
        "set_hmd_camera_control.ps1",
        "verify_hmd_camera_live_test.ps1",
        "verify_native_stereo_live_test.ps1",
        "summarize_native_stereo_run.ps1",
        "vr_test.ps1"
    )
    foreach ($Script in $Scripts) {
        $Tokens = $null
        $Errors = $null
        [void][System.Management.Automation.Language.Parser]::ParseFile(
            (Join-Path $SourceDirectory "tools\$Script"),
            [ref]$Tokens,
            [ref]$Errors)
        Assert-True ($Errors.Count -eq 0) "$Script has PowerShell syntax errors."
    }

    $VrTestSource = Get-Content -LiteralPath (Join-Path $SourceDirectory "tools\vr_test.ps1") -Raw
    Assert-True ($VrTestSource.Contains('cmake --build build-win32 --config Release')) `
        "vr_test does not build the expected release tree."
    Assert-True ($VrTestSource.Contains('build-win32\Release\d3d9_native_stereo.dll')) `
        "vr_test stages a native-stereo DLL from a different tree than it builds/tests."
    Assert-True (-not ($VrTestSource -match 'New-Item\s+-ItemType\s+File\s+-Path\s+\$ExBridgeMarkerPath')) `
        "vr_test re-enables the rejected D3D9Ex game path."

    $NativeStereoProxySource = Get-Content -LiteralPath (
        Join-Path $SourceDirectory "src\games\call_of_juarez\native_stereo_proxy.cpp") -Raw
    $SharedBridgeSource = Get-Content -LiteralPath (
        Join-Path $SourceDirectory "src\backends\openvr\d3d9_shared_texture_bridge.cpp") -Raw
    $FinalizeStart = $NativeStereoProxySource.IndexOf("void Finalize() noexcept")
    $PresenterStop = $NativeStereoProxySource.IndexOf("g_stereo.presenter.Stop();", $FinalizeStart)
    $CaptureShutdown = $NativeStereoProxySource.IndexOf("g_stereo.capture.Shutdown();", $FinalizeStart)
    Assert-True ($FinalizeStart -ge 0 -and $PresenterStop -ge 0 -and $CaptureShutdown -ge 0 -and
        $PresenterStop -lt $CaptureShutdown) `
        "Native-stereo shutdown destroys producer capture resources before the presenter releases leased frames."

    $ReadbackToggleStart = $NativeStereoProxySource.IndexOf(
        "void SetCaptureReadbackEnabled(void* context, const bool enabled) noexcept")
    $ReadbackToggleEnd = $NativeStereoProxySource.IndexOf(
        "bool QueryDiagnosticCounters(", $ReadbackToggleStart)
    Assert-True ($ReadbackToggleStart -ge 0 -and $ReadbackToggleEnd -gt $ReadbackToggleStart) `
        "Could not isolate SetCaptureReadbackEnabled for lifetime regression checking."
    $ReadbackToggleBody = $NativeStereoProxySource.Substring(
        $ReadbackToggleStart, $ReadbackToggleEnd - $ReadbackToggleStart)
    Assert-True (-not $ReadbackToggleBody.Contains("InvalidateResources()")) `
        "Disabling native-stereo capture invalidates producer resources while presenter leases may still be active."

    $BridgeShutdownStart = $SharedBridgeSource.IndexOf(
        "void D3D9SharedTextureBridge::Shutdown(ID3D11DeviceContext* context) noexcept")
    $BridgeShutdownEnd = $SharedBridgeSource.IndexOf(
        "D3D9SharedTextureBridgeStats D3D9SharedTextureBridge::stats()", $BridgeShutdownStart)
    Assert-True ($BridgeShutdownStart -ge 0 -and $BridgeShutdownEnd -gt $BridgeShutdownStart) `
        "Could not isolate D3D9SharedTextureBridge::Shutdown for lifetime regression checking."
    $BridgeShutdownBody = $SharedBridgeSource.Substring(
        $BridgeShutdownStart, $BridgeShutdownEnd - $BridgeShutdownStart)
    Assert-True ($BridgeShutdownBody.Contains("SynchronizeD3D11") -and
        $BridgeShutdownBody.Contains("D3D11SyncStrategy::event_query")) `
        "Shared-texture shutdown releases producer leases without a bounded D3D11 completion barrier."

    $StageSource = Get-Content -LiteralPath (Join-Path $SourceDirectory "tools\stage_d3d9_proxy.ps1") -Raw
    $UnstageSource = Get-Content -LiteralPath (Join-Path $SourceDirectory "tools\unstage_d3d9_proxy.ps1") -Raw
    Assert-True ($StageSource.Contains('[ValidateSet("full", "performance", "transport", "startup")]')) `
        "Native-stereo staging has no dedicated short transport validation profile."
    Assert-True ($VrTestSource.Contains('$ValidationProfile = if ($StartupOnly) { "startup" } elseif ($BodyIkAtStart) { "full" } else { "transport" }')) `
        "vr_test does not select the short transport profile by default."
    Assert-True ($StageSource.Contains('requireFlatTheaterUi = $IsNativeStereo -and $ValidationProfile -ne "transport"')) `
        "Transport validation still requires unrelated flat-theater UI acceptance."
    Assert-True ($StageSource.Contains('requireExplicitPassthroughDisable = $IsNativeStereo -and $ValidationProfile -ne "transport"')) `
        "Transport validation still requires an unrelated live tracking-disable step."
    Assert-True ($StageSource.Contains('requireStereoGeometry = $IsNativeStereo -and $ValidationProfile -ne "transport"')) `
        "Transport validation still requires camera/stereo geometry acceptance."
    $RequiredStageCheckpoints = @(
        "stage_proxy_backup_created",
        "stage_proxy_published",
        "stage_openvr_backup_created",
        "stage_openvr_published",
        "stage_openvr_input_backup_created",
        "stage_openvr_input_temporary_created",
        "stage_openvr_action_copied",
        "stage_openvr_binding_copied",
        "stage_openvr_input_published",
        "stage_camera_control_backup_created",
        "stage_camera_control_published",
        "stage_state_written",
        "stage_run_manifest_written",
        "stage_evidence_manifest_written"
    )
    foreach ($Checkpoint in $RequiredStageCheckpoints) {
        Assert-True ($StageSource.Contains("`"$Checkpoint`"")) `
            "Stage transaction is missing failure-injection checkpoint '$Checkpoint'."
    }
    $RequiredUnstageCheckpoints = @(
        "unstage_proxy_removed",
        "unstage_proxy_original_restored",
        "unstage_camera_control_removed",
        "unstage_camera_control_original_restored",
        "unstage_openvr_removed",
        "unstage_openvr_original_restored",
        "unstage_openvr_input_removed",
        "unstage_openvr_input_original_restored",
        "unstage_state_removed"
    )
    foreach ($Checkpoint in $RequiredUnstageCheckpoints) {
        Assert-True ($UnstageSource.Contains("`"$Checkpoint`"")) `
            "Unstage transaction is missing failure-injection checkpoint '$Checkpoint'."
    }

    # The real scripts must reject an active CoJ-named process before inspecting
    # or mutating any game asset. Use a copied system cmd.exe as a synthetic
    # process; this is not the game and requires no SteamVR/headset state.
    $ActiveProcessGame = Join-Path $TestRoot "active-process-game"
    New-Item -ItemType Directory -Path $ActiveProcessGame -Force | Out-Null
    $FakeCoJ = Join-Path $ActiveProcessGame "CoJ.exe"
    Copy-Item -LiteralPath (Join-Path $env:SystemRoot "System32\cmd.exe") -Destination $FakeCoJ
    $FakeProcess = Start-Process `
        -FilePath $FakeCoJ `
        -ArgumentList "/c ping 127.0.0.1 -n 20" `
        -WindowStyle Hidden `
        -PassThru
    try {
        for ($Attempt = 0; $Attempt -lt 20 -and -not (Get-Process -Name CoJ -ErrorAction SilentlyContinue); ++$Attempt) {
            Start-Sleep -Milliseconds 50
        }
        Assert-True ([bool](Get-Process -Name CoJ -ErrorAction SilentlyContinue)) `
            "Synthetic CoJ-named process did not start for active-process deployment coverage."
        $ActiveStageRejected = $false
        try {
            & (Join-Path $SourceDirectory "tools\stage_d3d9_proxy.ps1") -GameDirectory $ActiveProcessGame
        } catch {
            $ActiveStageRejected = $_.Exception.Message -match "Call of Juarez is running"
        }
        Assert-True $ActiveStageRejected `
            "Stage script did not reject an active CoJ-named process before mutation."
        $ActiveUnstageRejected = $false
        try {
            & (Join-Path $SourceDirectory "tools\unstage_d3d9_proxy.ps1") -GameDirectory $ActiveProcessGame
        } catch {
            $ActiveUnstageRejected = $_.Exception.Message -match "Call of Juarez is running"
        }
        Assert-True $ActiveUnstageRejected `
            "Unstage script did not reject an active CoJ-named process before mutation."
    } finally {
        if ($FakeProcess -and -not $FakeProcess.HasExited) {
            Stop-Process -Id $FakeProcess.Id -Force -ErrorAction SilentlyContinue
            $FakeProcess.WaitForExit(5000)
        }
    }

    . (Join-Path $SourceDirectory "tools\deployment_transaction.ps1")
    $TransactionRoot = Join-Path $TestRoot "deployment-transaction"
    New-Item -ItemType Directory -Path $TransactionRoot -Force | Out-Null

    $CleanGame = Join-Path $TransactionRoot "clean"
    New-Item -ItemType Directory -Path $CleanGame -Force | Out-Null
    $CleanStaged = Join-Path $CleanGame "d3d9.dll"
    [System.IO.File]::WriteAllBytes($CleanStaged, [byte[]](1, 2, 3, 4))
    $CleanStagedHash = Get-CojvrFileSha256 $CleanStaged
    [void](Write-CojvrDeploymentJournal $CleanGame "stage" "clean-run" "d3d9_forwarding" @(
        [ordered]@{
            role = "proxy"; kind = "file"; destination = "d3d9.dll"; backup = "d3d9.cojvr-backup.dll"
            hadOriginal = $false; originalSha256 = $null; stagedSha256 = $CleanStagedHash
        }
    ))
    Assert-True (Complete-CojvrDeploymentRecovery $CleanGame) `
        "Clean interrupted deployment was not recovered."
    Assert-True (-not (Test-Path -LiteralPath $CleanStaged)) `
        "Clean recovery did not remove the staged proxy."
    Assert-True (-not (Complete-CojvrDeploymentRecovery $CleanGame)) `
        "Repeated recovery did not become a no-op."

    $VerifiedInstallGame = Join-Path $TransactionRoot "verified-install"
    New-Item -ItemType Directory -Path $VerifiedInstallGame -Force | Out-Null
    $VerifiedInstallSource = Join-Path $VerifiedInstallGame "source.bin"
    $VerifiedInstallDestination = Join-Path $VerifiedInstallGame "installed.bin"
    $VerifiedInstallTemporary = Join-Path $VerifiedInstallGame "installed.cojvr-installing.bin"
    [System.IO.File]::WriteAllBytes($VerifiedInstallSource, [byte[]](41, 42, 43, 44))
    $VerifiedInstallHash = Get-CojvrFileSha256 $VerifiedInstallSource
    Install-CojvrVerifiedFile `
        $VerifiedInstallSource $VerifiedInstallDestination $VerifiedInstallTemporary $VerifiedInstallHash
    Assert-True ((Get-CojvrFileSha256 $VerifiedInstallDestination) -eq $VerifiedInstallHash -and
        -not (Test-Path -LiteralPath $VerifiedInstallTemporary)) `
        "Verified temporary file installation did not publish the expected artifact cleanly."

    $PreviousFailureTest = $env:COJVR_DEPLOYMENT_FAILURE_TEST
    $PreviousFailureCheckpoint = $env:COJVR_DEPLOYMENT_FAIL_AFTER
    try {
        $env:COJVR_DEPLOYMENT_FAILURE_TEST = "1"
        $env:COJVR_DEPLOYMENT_FAIL_AFTER = "file_published"
        $InjectedFailureGame = Join-Path $TransactionRoot "injected-failure"
        New-Item -ItemType Directory -Path $InjectedFailureGame -Force | Out-Null
        $InjectedSource = Join-Path $InjectedFailureGame "source.bin"
        $InjectedDestination = Join-Path $InjectedFailureGame "installed.bin"
        $InjectedTemporary = Join-Path $InjectedFailureGame "installed.cojvr-installing.bin"
        [System.IO.File]::WriteAllBytes($InjectedSource, [byte[]](60, 61, 62))
        $InjectedHash = Get-CojvrFileSha256 $InjectedSource
        $InjectedRejected = $false
        try {
            Install-CojvrVerifiedFile $InjectedSource $InjectedDestination $InjectedTemporary $InjectedHash
        } catch {
            $InjectedRejected = $true
        }
        Assert-True $InjectedRejected "Deployment failure injection did not interrupt publication."
    } finally {
        $env:COJVR_DEPLOYMENT_FAILURE_TEST = $PreviousFailureTest
        $env:COJVR_DEPLOYMENT_FAIL_AFTER = $PreviousFailureCheckpoint
    }

    $TemporaryGame = Join-Path $TransactionRoot "temporary-file"
    New-Item -ItemType Directory -Path $TemporaryGame -Force | Out-Null
    $TemporaryCandidate = Join-Path $TemporaryGame "d3d9.cojvr-installing.dll"
    [System.IO.File]::WriteAllBytes($TemporaryCandidate, [byte[]](45, 46, 47, 48))
    $TemporaryCandidateHash = Get-CojvrFileSha256 $TemporaryCandidate
    [void](Write-CojvrDeploymentJournal $TemporaryGame "stage" "temporary-run" "d3d9_forwarding" @(
        [ordered]@{
            role = "proxy"; kind = "file"; destination = "d3d9.dll"; backup = "d3d9.cojvr-backup.dll"
            temporary = "d3d9.cojvr-installing.dll"
            hadOriginal = $false; originalSha256 = $null; stagedSha256 = $TemporaryCandidateHash
        }
    ))
    [void](Complete-CojvrDeploymentRecovery $TemporaryGame)
    Assert-True (-not (Test-Path -LiteralPath $TemporaryCandidate)) `
        "Interrupted deployment recovery left a verified temporary file behind."

    $OriginalGame = Join-Path $TransactionRoot "original"
    New-Item -ItemType Directory -Path $OriginalGame -Force | Out-Null
    $OriginalDestination = Join-Path $OriginalGame "d3d9.dll"
    $OriginalBackup = Join-Path $OriginalGame "d3d9.cojvr-backup.dll"
    [System.IO.File]::WriteAllBytes($OriginalDestination, [byte[]](5, 6, 7, 8))
    $OriginalHash = Get-CojvrFileSha256 $OriginalDestination
    $CandidatePath = Join-Path $OriginalGame "candidate.bin"
    [System.IO.File]::WriteAllBytes($CandidatePath, [byte[]](9, 10, 11, 12))
    $CandidateHash = Get-CojvrFileSha256 $CandidatePath
    [void](Write-CojvrDeploymentJournal $OriginalGame "stage" "original-run" "d3d9_forwarding" @(
        [ordered]@{
            role = "proxy"; kind = "file"; destination = "d3d9.dll"; backup = "d3d9.cojvr-backup.dll"
            hadOriginal = $true; originalSha256 = $OriginalHash; stagedSha256 = $CandidateHash
        }
    ))
    Move-Item -LiteralPath $OriginalDestination -Destination $OriginalBackup
    Copy-Item -LiteralPath $CandidatePath -Destination $OriginalDestination
    [void](Complete-CojvrDeploymentRecovery $OriginalGame)
    Assert-True ((Get-CojvrFileSha256 $OriginalDestination) -eq $OriginalHash -and
        -not (Test-Path -LiteralPath $OriginalBackup)) `
        "Interrupted deployment did not restore the original proxy."

    $DirectoryGame = Join-Path $TransactionRoot "directory"
    $InputDirectory = Join-Path $DirectoryGame "cojvr_openvr_input"
    $InputBackup = Join-Path $DirectoryGame "cojvr_openvr_input.cojvr-backup"
    New-Item -ItemType Directory -Path (Join-Path $InputDirectory "bindings") -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $InputDirectory "custom.json") -Encoding UTF8 -Value "original"
    $OriginalInputManifest = @(Get-CojvrDirectoryManifest $InputDirectory)
    $StagedActionBytes = [byte[]](20, 21, 22)
    $StagedBindingBytes = [byte[]](23, 24, 25)
    $ActionTemp = Join-Path $DirectoryGame "action.tmp"
    $BindingTemp = Join-Path $DirectoryGame "binding.tmp"
    [System.IO.File]::WriteAllBytes($ActionTemp, $StagedActionBytes)
    [System.IO.File]::WriteAllBytes($BindingTemp, $StagedBindingBytes)
    $StagedInputManifest = @(
        [ordered]@{ path = "actions.json"; sha256 = Get-CojvrFileSha256 $ActionTemp },
        [ordered]@{ path = "bindings/psvr2_sense.json"; sha256 = Get-CojvrFileSha256 $BindingTemp }
    )
    [void](Write-CojvrDeploymentJournal $DirectoryGame "stage" "directory-run" "d3d9_native_stereo" @(
        [ordered]@{
            role = "openvr_input"; kind = "directory"; destination = "cojvr_openvr_input"; backup = "cojvr_openvr_input.cojvr-backup"
            hadOriginal = $true; originalManifest = $OriginalInputManifest; stagedManifest = $StagedInputManifest
        }
    ))
    Move-Item -LiteralPath $InputDirectory -Destination $InputBackup
    New-Item -ItemType Directory -Path (Join-Path $InputDirectory "bindings") -Force | Out-Null
    [System.IO.File]::WriteAllBytes((Join-Path $InputDirectory "actions.json"), $StagedActionBytes)
    [void](Complete-CojvrDeploymentRecovery $DirectoryGame)
    Assert-True (Test-CojvrDirectoryMatchesManifest $InputDirectory $OriginalInputManifest) `
        "Interrupted directory deployment did not restore the original OpenVR input directory."

    $TemporaryDirectoryGame = Join-Path $TransactionRoot "temporary-directory"
    $TemporaryInputDirectory = Join-Path $TemporaryDirectoryGame "cojvr_openvr_input.cojvr-installing"
    New-Item -ItemType Directory -Path (Join-Path $TemporaryInputDirectory "bindings") -Force | Out-Null
    [System.IO.File]::WriteAllBytes((Join-Path $TemporaryInputDirectory "actions.json"), $StagedActionBytes)
    [void](Write-CojvrDeploymentJournal $TemporaryDirectoryGame "stage" "temporary-directory-run" "d3d9_native_stereo" @(
        [ordered]@{
            role = "openvr_input"; kind = "directory"; destination = "cojvr_openvr_input"; backup = "cojvr_openvr_input.cojvr-backup"
            temporary = "cojvr_openvr_input.cojvr-installing"
            hadOriginal = $false; originalManifest = @(); stagedManifest = $StagedInputManifest
        }
    ))
    [void](Complete-CojvrDeploymentRecovery $TemporaryDirectoryGame)
    Assert-True (-not (Test-Path -LiteralPath $TemporaryInputDirectory)) `
        "Interrupted deployment recovery left a known partial temporary directory behind."

    $MissingBackupGame = Join-Path $TransactionRoot "missing-backup"
    New-Item -ItemType Directory -Path $MissingBackupGame -Force | Out-Null
    $MissingOriginal = Join-Path $MissingBackupGame "original.tmp"
    $MissingCandidate = Join-Path $MissingBackupGame "candidate.tmp"
    [System.IO.File]::WriteAllBytes($MissingOriginal, [byte[]](50, 51, 52))
    [System.IO.File]::WriteAllBytes($MissingCandidate, [byte[]](53, 54, 55))
    $MissingOriginalHash = Get-CojvrFileSha256 $MissingOriginal
    $MissingCandidateHash = Get-CojvrFileSha256 $MissingCandidate
    [void](Write-CojvrDeploymentJournal $MissingBackupGame "stage" "missing-backup-run" "d3d9_forwarding" @(
        [ordered]@{
            role = "proxy"; kind = "file"; destination = "d3d9.dll"; backup = "d3d9.cojvr-backup.dll"
            temporary = "d3d9.cojvr-installing.dll"
            hadOriginal = $true; originalSha256 = $MissingOriginalHash; stagedSha256 = $MissingCandidateHash
        }
    ))
    $MissingBackupRejected = $false
    try {
        [void](Complete-CojvrDeploymentRecovery $MissingBackupGame)
    } catch {
        $MissingBackupRejected = $true
    }
    Assert-True $MissingBackupRejected `
        "Deployment recovery accepted a missing original backup with no provable destination."
    Assert-True (Test-Path -LiteralPath (Join-Path $MissingBackupGame ".cojvr-deployment-transaction.json")) `
        "Missing-backup recovery removed the journal needed for diagnosis."

    $ChangedGame = Join-Path $TransactionRoot "changed"
    New-Item -ItemType Directory -Path $ChangedGame -Force | Out-Null
    $ChangedDestination = Join-Path $ChangedGame "d3d9.dll"
    $ChangedBackup = Join-Path $ChangedGame "d3d9.cojvr-backup.dll"
    [System.IO.File]::WriteAllBytes($ChangedDestination, [byte[]](30, 31, 32))
    $ChangedOriginalHash = Get-CojvrFileSha256 $ChangedDestination
    [System.IO.File]::WriteAllBytes((Join-Path $ChangedGame "candidate.tmp"), [byte[]](33, 34, 35))
    $ChangedCandidateHash = Get-CojvrFileSha256 (Join-Path $ChangedGame "candidate.tmp")
    [void](Write-CojvrDeploymentJournal $ChangedGame "stage" "changed-run" "d3d9_forwarding" @(
        [ordered]@{
            role = "proxy"; kind = "file"; destination = "d3d9.dll"; backup = "d3d9.cojvr-backup.dll"
            hadOriginal = $true; originalSha256 = $ChangedOriginalHash; stagedSha256 = $ChangedCandidateHash
        }
    ))
    Move-Item -LiteralPath $ChangedDestination -Destination $ChangedBackup
    [System.IO.File]::WriteAllBytes($ChangedDestination, [byte[]](99, 98, 97))
    $ExternalChangeRejected = $false
    try {
        [void](Complete-CojvrDeploymentRecovery $ChangedGame)
    } catch {
        $ExternalChangeRejected = $true
    }
    Assert-True $ExternalChangeRejected `
        "Deployment recovery overwrote an externally changed destination."
    Assert-True (Test-Path -LiteralPath (Join-Path $ChangedGame ".cojvr-deployment-transaction.json")) `
        "Failed recovery removed the journal needed for manual diagnosis."
    $WrapperTokens = $null
    $WrapperErrors = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile(
        (Join-Path $SourceDirectory "tests\run_proxy_smoke.ps1"),
        [ref]$WrapperTokens,
        [ref]$WrapperErrors)
    Assert-True ($WrapperErrors.Count -eq 0) "run_proxy_smoke.ps1 has PowerShell syntax errors."

    $ArtifactDirectory = Join-Path $TestRoot "artifacts"
    New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null
    $ProxyPath = Join-Path $ArtifactDirectory "d3d9_openvr_flat.dll"
    $OpenVrPath = Join-Path $ArtifactDirectory "openvr_api.dll"
    [System.IO.File]::WriteAllBytes($ProxyPath, [byte[]](0x43, 0x4F, 0x4A, 0x56, 0x52))
    [System.IO.File]::WriteAllBytes($OpenVrPath, [byte[]](0x4F, 0x50, 0x45, 0x4E, 0x56, 0x52))
    $BuildManifestPath = Join-Path $ArtifactDirectory "d3d9_openvr_flat.build-manifest.json"

    & (Join-Path $SourceDirectory "tools\new_build_manifest.ps1") `
        -RepositoryRoot $SourceDirectory `
        -Configuration Release `
        -DiagnosticMode d3d9_openvr_flat `
        -ProxyPath $ProxyPath `
        -OpenVrDllPath $OpenVrPath `
        -OutputPath $BuildManifestPath `
        -AllowDirty

    $BuildManifest = Get-Content -LiteralPath $BuildManifestPath -Raw | ConvertFrom-Json
    Assert-True ($BuildManifest.manifestType -eq "cojvr-build") "Build manifest type is missing."
    Assert-True ($BuildManifest.manifestId -match "^[A-F0-9]{64}$") "Build manifest ID is not a SHA-256."
    Assert-True ($BuildManifest.source.headCommit -match "^[a-f0-9]{40}$") "Source commit was not recorded."
    Assert-True ($BuildManifest.build.platform -eq "Win32") "Win32 build identity was not recorded."
    Assert-True (@($BuildManifest.artifacts).Count -eq 2) "Expected proxy and OpenVR artifacts."

    $HmdProxyPath = Join-Path $ArtifactDirectory "d3d9_hmd_camera.dll"
    Copy-Item -LiteralPath $ProxyPath -Destination $HmdProxyPath
    $HmdBuildManifestPath = Join-Path $ArtifactDirectory "d3d9_hmd_camera.build-manifest.json"
    & (Join-Path $SourceDirectory "tools\new_build_manifest.ps1") `
        -RepositoryRoot $SourceDirectory `
        -Configuration Release `
        -DiagnosticMode d3d9_hmd_camera `
        -ProxyPath $HmdProxyPath `
        -OpenVrDllPath $OpenVrPath `
        -OutputPath $HmdBuildManifestPath `
        -AllowDirty
    $HmdBuildManifest = Get-Content -LiteralPath $HmdBuildManifestPath -Raw | ConvertFrom-Json
    Assert-True ($HmdBuildManifest.build.diagnosticMode -eq "d3d9_hmd_camera") `
        "HMD camera build mode was not recorded."
    Assert-True (@($HmdBuildManifest.artifacts).Count -eq 2) `
        "HMD camera manifest did not bind proxy and OpenVR runtime."

    $StereoProxyPath = Join-Path $ArtifactDirectory "d3d9_native_stereo.dll"
    Copy-Item -LiteralPath $ProxyPath -Destination $StereoProxyPath
    $StereoInputDirectory = Join-Path $ArtifactDirectory "cojvr_openvr_input"
    New-Item -ItemType Directory -Path (Join-Path $StereoInputDirectory "bindings") -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $SourceDirectory "assets\openvr\actions.json") `
        -Destination (Join-Path $StereoInputDirectory "actions.json")
    Copy-Item -LiteralPath (Join-Path $SourceDirectory "assets\openvr\bindings\psvr2_sense.json") `
        -Destination (Join-Path $StereoInputDirectory "bindings\psvr2_sense.json")
    $OpenVrActions = Get-Content -LiteralPath (Join-Path $StereoInputDirectory "actions.json") -Raw | ConvertFrom-Json
    $OpenVrSenseBinding = Get-Content -LiteralPath (Join-Path $StereoInputDirectory "bindings\psvr2_sense.json") -Raw | ConvertFrom-Json
    $ActionNames = @($OpenVrActions.actions | ForEach-Object { [string]$_.name })
    foreach ($RequiredAction in @(
        "/actions/global/in/recenter",
        "/actions/global/in/ui_select_left",
        "/actions/global/in/ui_select_right",
        "/actions/global/in/ui_accept",
        "/actions/global/in/ui_back",
        "/actions/global/in/left_hand_grip_pose",
        "/actions/global/in/right_hand_grip_pose",
        "/actions/global/in/left_hand_aim_pose",
        "/actions/global/in/right_hand_aim_pose",
        "/actions/gameplay/in/move",
        "/actions/gameplay/in/turn",
        "/actions/gameplay/in/fire_left",
        "/actions/gameplay/in/fire_right",
        "/actions/gameplay/in/jump",
        "/actions/gameplay/in/reload",
        "/actions/gameplay/in/run",
        "/actions/gameplay/in/crouch",
        "/actions/gameplay/in/interact",
        "/actions/gameplay/in/weapon_next",
        "/actions/gameplay/in/weapon_previous",
        "/actions/gameplay/in/kick")) {
        Assert-True ($ActionNames -contains $RequiredAction) `
            "OpenVR action manifest is missing required action '$RequiredAction'."
    }
    $GameplayBinding = $OpenVrSenseBinding.bindings.PSObject.Properties["/actions/gameplay"].Value
    $GlobalBinding = $OpenVrSenseBinding.bindings.PSObject.Properties["/actions/global"].Value
    $GameplaySources = @($GameplayBinding.sources)
    $GlobalSources = @($GlobalBinding.sources)
    $GlobalPoses = @($GlobalBinding.poses)
    function Assert-SenseBinding([string]$Path, [string]$InputName, [string]$Output) {
        $Matches = @(($GameplaySources + $GlobalSources) | Where-Object {
            [string]$_.path -eq $Path -and
            $null -ne $_.inputs.PSObject.Properties[$InputName] -and
            [string]$_.inputs.PSObject.Properties[$InputName].Value.output -eq $Output
        })
        Assert-True ($Matches.Count -eq 1) `
            "PS VR2 Sense binding '$($Path)/$InputName' does not map exactly once to '$Output'."
    }
    function Assert-SensePose([string]$Path, [string]$Output) {
        $Matches = @($GlobalPoses | Where-Object {
            [string]$_.path -eq $Path -and [string]$_.output -eq $Output
        })
        Assert-True ($Matches.Count -eq 1) `
            "PS VR2 Sense pose '$Path' does not map exactly once to '$Output'."
    }
    Assert-SenseBinding "/user/hand/left/input/create" "click" "/actions/global/in/recenter"
    Assert-SenseBinding "/user/hand/left/input/l2" "click" "/actions/global/in/ui_select_left"
    Assert-SenseBinding "/user/hand/right/input/r2" "click" "/actions/global/in/ui_select_right"
    Assert-SenseBinding "/user/hand/right/input/cross" "click" "/actions/global/in/ui_accept"
    Assert-SenseBinding "/user/hand/right/input/circle" "click" "/actions/global/in/ui_back"
    Assert-SensePose "/user/hand/left/pose/handgrip" "/actions/global/in/left_hand_grip_pose"
    Assert-SensePose "/user/hand/right/pose/handgrip" "/actions/global/in/right_hand_grip_pose"
    Assert-SensePose "/user/hand/left/pose/tip" "/actions/global/in/left_hand_aim_pose"
    Assert-SensePose "/user/hand/right/pose/tip" "/actions/global/in/right_hand_aim_pose"
    Assert-SenseBinding "/user/hand/left/input/left_stick" "position" "/actions/gameplay/in/move"
    Assert-SenseBinding "/user/hand/right/input/right_stick" "position" "/actions/gameplay/in/turn"
    Assert-SenseBinding "/user/hand/left/input/l2" "click" "/actions/gameplay/in/fire_left"
    Assert-SenseBinding "/user/hand/right/input/r2" "click" "/actions/gameplay/in/fire_right"
    Assert-SenseBinding "/user/hand/right/input/cross" "click" "/actions/gameplay/in/jump"
    Assert-SenseBinding "/user/hand/left/input/square" "click" "/actions/gameplay/in/reload"
    $StereoBuildManifestPath = Join-Path $ArtifactDirectory "d3d9_native_stereo.build-manifest.json"
    & (Join-Path $SourceDirectory "tools\new_build_manifest.ps1") `
        -RepositoryRoot $SourceDirectory `
        -Configuration Release `
        -DiagnosticMode d3d9_native_stereo `
        -ProxyPath $StereoProxyPath `
        -OpenVrDllPath $OpenVrPath `
        -OutputPath $StereoBuildManifestPath `
        -AllowDirty
    $StereoBuildManifest = Get-Content -LiteralPath $StereoBuildManifestPath -Raw | ConvertFrom-Json
    Assert-True ($StereoBuildManifest.build.diagnosticMode -eq "d3d9_native_stereo") `
        "Native-stereo build mode was not recorded."
    Assert-True (@($StereoBuildManifest.artifacts).Count -eq 4) `
        "Native-stereo manifest did not bind proxy, OpenVR runtime and controller input assets."
    Assert-True (@($StereoBuildManifest.artifacts | Where-Object { $_.role -eq "openvr_action_manifest" }).Count -eq 1) `
        "Native-stereo manifest did not bind the OpenVR action manifest."
    Assert-True (@($StereoBuildManifest.artifacts | Where-Object { $_.role -eq "openvr_binding_psvr2_sense" }).Count -eq 1) `
        "Native-stereo manifest did not bind the PS VR2 Sense profile."

    $GameDirectory = Join-Path $TestRoot "game"
    $RunId = "host-test-run"
    $RunDirectory = Join-Path $GameDirectory ".cojvr-evidence\runs\$RunId"
    New-Item -ItemType Directory -Path $RunDirectory -Force | Out-Null
    Copy-Item -LiteralPath $ProxyPath -Destination (Join-Path $GameDirectory "d3d9.dll")
    Copy-Item -LiteralPath $OpenVrPath -Destination (Join-Path $GameDirectory "openvr_api.dll")
    Copy-Item -LiteralPath $BuildManifestPath -Destination (Join-Path $RunDirectory "build-manifest.json")

    $ProxyHash = (Get-FileHash -LiteralPath (Join-Path $GameDirectory "d3d9.dll") -Algorithm SHA256).Hash
    $OpenVrHash = (Get-FileHash -LiteralPath (Join-Path $GameDirectory "openvr_api.dll") -Algorithm SHA256).Hash
    $BuildManifestHash = (Get-FileHash -LiteralPath $BuildManifestPath -Algorithm SHA256).Hash
    $RunManifest = [ordered]@{
        schemaVersion = 1
        manifestType = "cojvr-run"
        runId = $RunId
        buildManifestId = [string]$BuildManifest.manifestId
        buildManifestSha256 = $BuildManifestHash
        diagnosticMode = "d3d9_openvr_flat"
        validation = [ordered]@{ requireSteamOverlayAbsent = $true }
        deployment = @(
            [ordered]@{ role = "proxy"; destination = "d3d9.dll"; sha256 = $ProxyHash },
            [ordered]@{ role = "openvr_runtime"; destination = "openvr_api.dll"; sha256 = $OpenVrHash }
        )
    }
    Write-Utf8Json (Join-Path $GameDirectory ".cojvr-run.json") $RunManifest
    Write-Utf8Json (Join-Path $GameDirectory ".cojvr-d3d9-stage.json") ([ordered]@{
        runId = $RunId
        buildManifestId = [string]$BuildManifest.manifestId
        buildManifestSha256 = $BuildManifestHash
        diagnosticMode = "d3d9_openvr_flat"
    })
    Write-Utf8Json (Join-Path $GameDirectory ".cojvr-openvr-flat-stage.json") ([ordered]@{ runId = $RunId })
    Set-Content -LiteralPath (Join-Path $GameDirectory "cojvr.log") -Encoding UTF8 -Value @(
        "run_start: run_id=$RunId build_manifest_id=$($BuildManifest.manifestId) pid=123",
        "diagnostic event"
    )

    $Provenance = & (Join-Path $SourceDirectory "tools\get_run_provenance.ps1") `
        -GameDirectory $GameDirectory `
        -ExpectedDiagnosticMode "d3d9_openvr_flat"
    Assert-True ($Provenance.RunId -eq $RunId) "Verifier did not return the bound run ID."
    Assert-True ([bool]$Provenance.Run.validation.requireSteamOverlayAbsent) `
        "Run validation requirements were not preserved by provenance."

    Add-Content -LiteralPath (Join-Path $GameDirectory "cojvr.log") -Encoding UTF8 -Value `
        "run_start: run_id=$RunId build_manifest_id=$($BuildManifest.manifestId) pid=124"
    $DuplicateRunStartRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\get_run_provenance.ps1") `
            -GameDirectory $GameDirectory `
            -ExpectedDiagnosticMode "d3d9_openvr_flat" | Out-Null
    } catch {
        $DuplicateRunStartRejected = $true
    }
    Assert-True $DuplicateRunStartRejected "Provenance accepted a run ID reused across process starts."
    Set-Content -LiteralPath (Join-Path $GameDirectory "cojvr.log") -Encoding UTF8 -Value @(
        "run_start: run_id=$RunId build_manifest_id=$($BuildManifest.manifestId) pid=123",
        "diagnostic event"
    )

    $PackagePath = Join-Path $TestRoot "$RunId.zip"
    & (Join-Path $SourceDirectory "tools\collect_run_evidence.ps1") `
        -GameDirectory $GameDirectory `
        -OutputPath $PackagePath
    Assert-True (Test-Path -LiteralPath $PackagePath -PathType Leaf) "Evidence package was not created."

    $EvidenceManifestPath = Join-Path $RunDirectory "evidence-manifest.json"
    $EvidenceManifest = Get-Content -LiteralPath $EvidenceManifestPath -Raw | ConvertFrom-Json
    Assert-True ([bool]$EvidenceManifest.runtimeStarted) "Run start was not correlated."
    Assert-True ([bool]$EvidenceManifest.incomplete) "Missing run_end was not marked incomplete."
    Assert-True (-not (@($EvidenceManifest.deploymentChecks) | Where-Object { -not $_.matches })) `
        "Matching deployed artifacts were not verified."

    Add-Content -LiteralPath (Join-Path $GameDirectory "cojvr.log") -Encoding UTF8 -Value "run_end: run_id=$RunId"
    & (Join-Path $SourceDirectory "tools\collect_run_evidence.ps1") `
        -GameDirectory $GameDirectory `
        -OutputPath $PackagePath
    $EvidenceManifest = Get-Content -LiteralPath $EvidenceManifestPath -Raw | ConvertFrom-Json
    Assert-True ([bool]$EvidenceManifest.runtimeEnded) "Run end was not correlated."
    Assert-True (-not [bool]$EvidenceManifest.incomplete) "Finalized run was marked incomplete."

    [System.IO.File]::WriteAllBytes((Join-Path $GameDirectory "d3d9.dll"), [byte[]](0x00))
    & (Join-Path $SourceDirectory "tools\collect_run_evidence.ps1") `
        -GameDirectory $GameDirectory `
        -OutputPath $PackagePath
    $EvidenceManifest = Get-Content -LiteralPath $EvidenceManifestPath -Raw | ConvertFrom-Json
    $ProxyCheck = @($EvidenceManifest.deploymentChecks | Where-Object { $_.destination -eq "d3d9.dll" })
    Assert-True ($ProxyCheck.Count -eq 1 -and -not [bool]$ProxyCheck[0].matches) `
        "Changed deployed proxy was not reported."

    $CameraControlGame = Join-Path $TestRoot "camera-control-game"
    New-Item -ItemType Directory -Path $CameraControlGame -Force | Out-Null
    Write-Utf8Json (Join-Path $CameraControlGame ".cojvr-d3d9-stage.json") ([ordered]@{
        diagnosticMode = "d3d9_camera_probe"
    })
    & (Join-Path $SourceDirectory "tools\set_camera_probe_control.ps1") `
        -GameDirectory $CameraControlGame `
        -Mode enable `
        -FovDegrees 105 `
        -YawDegrees 15 `
        -PitchDegrees -5
    $CameraControl = Get-Content -LiteralPath `
        (Join-Path $CameraControlGame "cojvr-camera-control.json") -Raw | ConvertFrom-Json
    Assert-True ([bool]$CameraControl.enabled) "Camera control enable command was not written."
    Assert-True ([double]$CameraControl.fovDegrees -eq 105.0) "Camera control FOV was not preserved."
    Assert-True ([double]$CameraControl.yawDegrees -eq 15.0) "Camera control yaw was not preserved."
    Assert-True ([double]$CameraControl.pitchDegrees -eq -5.0) "Camera control pitch was not preserved."

    & (Join-Path $SourceDirectory "tools\set_camera_probe_control.ps1") `
        -GameDirectory $CameraControlGame `
        -Mode disable
    $CameraControl = Get-Content -LiteralPath `
        (Join-Path $CameraControlGame "cojvr-camera-control.json") -Raw | ConvertFrom-Json
    Assert-True (-not [bool]$CameraControl.enabled) "Camera control disable command was not written."

    Write-Utf8Json (Join-Path $CameraControlGame ".cojvr-d3d9-stage.json") ([ordered]@{
        diagnosticMode = "d3d9_forwarding"
    })
    $WrongModeRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\set_camera_probe_control.ps1") `
            -GameDirectory $CameraControlGame `
            -Mode enable
    } catch {
        $WrongModeRejected = $true
    }
    Assert-True $WrongModeRejected "Camera control accepted a non-camera-probe staging mode."

    $HmdControlGame = Join-Path $TestRoot "hmd-control-game"
    New-Item -ItemType Directory -Path $HmdControlGame -Force | Out-Null
    Write-Utf8Json (Join-Path $HmdControlGame ".cojvr-d3d9-stage.json") ([ordered]@{
        diagnosticMode = "d3d9_hmd_camera"
    })
    & (Join-Path $SourceDirectory "tools\set_hmd_camera_control.ps1") `
        -GameDirectory $HmdControlGame `
        -Mode enable
    $HmdControl = Get-Content -LiteralPath `
        (Join-Path $HmdControlGame "cojvr-camera-control.json") -Raw | ConvertFrom-Json
    Assert-True ([bool]$HmdControl.trackingEnabled -and [bool]$HmdControl.recenter) `
        "HMD enable did not request tracking plus base-orientation capture."
    Assert-True (-not [bool]$HmdControl.enabled) `
        "HMD control accidentally enabled the manual diagnostic orientation source."

    & (Join-Path $SourceDirectory "tools\set_hmd_camera_control.ps1") `
        -GameDirectory $HmdControlGame `
        -Mode disable
    $HmdControl = Get-Content -LiteralPath `
        (Join-Path $HmdControlGame "cojvr-camera-control.json") -Raw | ConvertFrom-Json
    Assert-True (-not [bool]$HmdControl.trackingEnabled) `
        "HMD disable did not restore tracking passthrough."

    Write-Utf8Json (Join-Path $HmdControlGame ".cojvr-d3d9-stage.json") ([ordered]@{
        diagnosticMode = "d3d9_native_stereo"
    })
    & (Join-Path $SourceDirectory "tools\set_hmd_camera_control.ps1") `
        -GameDirectory $HmdControlGame `
        -Mode enable
    $StereoControl = Get-Content -LiteralPath `
        (Join-Path $HmdControlGame "cojvr-camera-control.json") -Raw | ConvertFrom-Json
    Assert-True ([bool]$StereoControl.trackingEnabled -and [bool]$StereoControl.recenter) `
        "Native-stereo control did not reuse the HMD tracking/recenter contract."

    $HmdVerifierGame = Join-Path $TestRoot "hmd-verifier-game"
    $HmdVerifierRunId = "host-test-hmd-camera"
    $HmdVerifierRunDirectory = Join-Path $HmdVerifierGame ".cojvr-evidence\runs\$HmdVerifierRunId"
    New-Item -ItemType Directory -Path $HmdVerifierRunDirectory -Force | Out-Null
    Copy-Item -LiteralPath $HmdProxyPath -Destination (Join-Path $HmdVerifierGame "d3d9.dll")
    Copy-Item -LiteralPath $OpenVrPath -Destination (Join-Path $HmdVerifierGame "openvr_api.dll")
    Copy-Item -LiteralPath $HmdBuildManifestPath -Destination (Join-Path $HmdVerifierRunDirectory "build-manifest.json")

    $HmdVerifierProxyHash = (Get-FileHash -LiteralPath (Join-Path $HmdVerifierGame "d3d9.dll") -Algorithm SHA256).Hash
    $HmdVerifierOpenVrHash = (Get-FileHash -LiteralPath (Join-Path $HmdVerifierGame "openvr_api.dll") -Algorithm SHA256).Hash
    $HmdVerifierBuildManifestHash = (Get-FileHash -LiteralPath $HmdBuildManifestPath -Algorithm SHA256).Hash
    Write-Utf8Json (Join-Path $HmdVerifierGame ".cojvr-run.json") ([ordered]@{
        schemaVersion = 1
        manifestType = "cojvr-run"
        runId = $HmdVerifierRunId
        buildManifestId = [string]$HmdBuildManifest.manifestId
        buildManifestSha256 = $HmdVerifierBuildManifestHash
        diagnosticMode = "d3d9_hmd_camera"
        game = [ordered]@{
            executable = [ordered]@{
                sha256 = "5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE"
                knownExactBuild = $true
            }
            engine = [ordered]@{
                sha256 = "DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8"
                knownInspectedBuild = $true
            }
        }
        deployment = @(
            [ordered]@{ role = "proxy"; destination = "d3d9.dll"; sha256 = $HmdVerifierProxyHash },
            [ordered]@{ role = "openvr_runtime"; destination = "openvr_api.dll"; sha256 = $HmdVerifierOpenVrHash }
        )
    })
    Write-Utf8Json (Join-Path $HmdVerifierGame ".cojvr-d3d9-stage.json") ([ordered]@{
        runId = $HmdVerifierRunId
        buildManifestId = [string]$HmdBuildManifest.manifestId
        buildManifestSha256 = $HmdVerifierBuildManifestHash
        diagnosticMode = "d3d9_hmd_camera"
    })

    $HmdOrientation1 = "camera_probe_event: event=camera_hmd_orientation_applied result=ok pose_sequence=2 yaw_degrees=10 pitch_degrees=0 natural_determinant=1 applied_determinant=1 native_homogeneous_layout=true source_world_homogeneous_layout=true source_view_homogeneous_layout=true injected_view_homogeneous_layout=true render_basis_observed=true render_basis_changed=true view_matrix_observed=true view_matrix_changed=true view_projection_changed=true restored=true;renderer_camera_match=true"
    $HmdOrientation2 = "camera_probe_event: event=camera_hmd_orientation_applied result=ok pose_sequence=3 yaw_degrees=0 pitch_degrees=5 natural_determinant=1 applied_determinant=1 native_homogeneous_layout=true source_world_homogeneous_layout=true source_view_homogeneous_layout=true injected_view_homogeneous_layout=true render_basis_observed=true render_basis_changed=true view_matrix_observed=true view_matrix_changed=true view_projection_changed=true restored=true;renderer_camera_match=true"
    $HmdVerifierLog = @(
        "run_start: run_id=$HmdVerifierRunId build_manifest_id=$($HmdBuildManifest.manifestId) pid=456",
        "camera_hmd_pose_source: status=started backend=openvr tracking_space=standing",
        "camera_probe_bootstrap: status=installed system_d3d9=expected",
        "camera_probe_event: event=camera_probe_install result=installed pose_source=available",
        "camera_probe_event: event=camera_hmd_recentered result=ok",
        "camera_probe_event: event=camera_probe_control_loaded result=accepted generation=1 tracking_enabled=true",
        $HmdOrientation1,
        $HmdOrientation2,
        "camera_probe_event: event=camera_probe_control_loaded result=accepted generation=2 tracking_enabled=false",
        "camera_probe_event: event=camera_probe_passthrough result=disabled",
        "camera_probe_event: event=camera_probe_restore result=restored",
        "camera_hmd_pose_source: status=stopped",
        "run_end: run_id=$HmdVerifierRunId"
    )
    Set-Content -LiteralPath (Join-Path $HmdVerifierGame "cojvr.log") -Encoding UTF8 -Value $HmdVerifierLog
    & (Join-Path $SourceDirectory "tools\verify_hmd_camera_live_test.ps1") `
        -GameDirectory $HmdVerifierGame | Out-Null

    $HmdVerifierInvalidNatural = @($HmdVerifierLog)
    $HmdVerifierInvalidNatural[6] = $HmdOrientation1 -replace "natural_determinant=1", "natural_determinant=0.5"
    Set-Content -LiteralPath (Join-Path $HmdVerifierGame "cojvr.log") -Encoding UTF8 -Value $HmdVerifierInvalidNatural
    $InvalidNaturalRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_hmd_camera_live_test.ps1") `
            -GameDirectory $HmdVerifierGame | Out-Null
    } catch {
        $InvalidNaturalRejected = $true
    }
    Assert-True $InvalidNaturalRejected `
        "HMD verifier accepted a non-rigid natural camera basis."

    $HmdVerifierInvalidView = @($HmdVerifierLog)
    $HmdVerifierInvalidView[6] = $HmdOrientation1 -replace `
        "source_view_homogeneous_layout=true", "source_view_homogeneous_layout=false"
    Set-Content -LiteralPath (Join-Path $HmdVerifierGame "cojvr.log") -Encoding UTF8 -Value $HmdVerifierInvalidView
    $InvalidViewRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_hmd_camera_live_test.ps1") `
            -GameDirectory $HmdVerifierGame | Out-Null
    } catch {
        $InvalidViewRejected = $true
    }
    Assert-True $InvalidViewRejected `
        "HMD verifier accepted an invalid source-view homogeneous layout."

    $StereoVerifierGame = Join-Path $TestRoot "stereo-verifier-game"
    $StereoVerifierRunId = "host-test-native-stereo"
    $StereoVerifierRunDirectory = Join-Path $StereoVerifierGame ".cojvr-evidence\runs\$StereoVerifierRunId"
    New-Item -ItemType Directory -Path $StereoVerifierRunDirectory -Force | Out-Null
    Copy-Item -LiteralPath $StereoProxyPath -Destination (Join-Path $StereoVerifierGame "d3d9.dll")
    Copy-Item -LiteralPath $OpenVrPath -Destination (Join-Path $StereoVerifierGame "openvr_api.dll")
    New-Item -ItemType Directory -Path (Join-Path $StereoVerifierGame "cojvr_openvr_input\bindings") -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $StereoInputDirectory "actions.json") `
        -Destination (Join-Path $StereoVerifierGame "cojvr_openvr_input\actions.json")
    Copy-Item -LiteralPath (Join-Path $StereoInputDirectory "bindings\psvr2_sense.json") `
        -Destination (Join-Path $StereoVerifierGame "cojvr_openvr_input\bindings\psvr2_sense.json")
    Copy-Item -LiteralPath $StereoBuildManifestPath -Destination (Join-Path $StereoVerifierRunDirectory "build-manifest.json")

    $StereoVerifierProxyHash = (Get-FileHash -LiteralPath (Join-Path $StereoVerifierGame "d3d9.dll") -Algorithm SHA256).Hash
    $StereoVerifierOpenVrHash = (Get-FileHash -LiteralPath (Join-Path $StereoVerifierGame "openvr_api.dll") -Algorithm SHA256).Hash
    $StereoVerifierActionManifestHash = (Get-FileHash -LiteralPath (Join-Path $StereoVerifierGame "cojvr_openvr_input\actions.json") -Algorithm SHA256).Hash
    $StereoVerifierSenseBindingHash = (Get-FileHash -LiteralPath (Join-Path $StereoVerifierGame "cojvr_openvr_input\bindings\psvr2_sense.json") -Algorithm SHA256).Hash
    $StereoVerifierBuildManifestHash = (Get-FileHash -LiteralPath $StereoBuildManifestPath -Algorithm SHA256).Hash
    Write-Utf8Json (Join-Path $StereoVerifierGame ".cojvr-run.json") ([ordered]@{
        schemaVersion = 1
        manifestType = "cojvr-run"
        runId = $StereoVerifierRunId
        buildManifestId = [string]$StereoBuildManifest.manifestId
        buildManifestSha256 = $StereoVerifierBuildManifestHash
        diagnosticMode = "d3d9_native_stereo"
        game = [ordered]@{
            executable = [ordered]@{
                sha256 = "5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE"
                knownExactBuild = $true
            }
            engine = [ordered]@{
                sha256 = "DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8"
                knownInspectedBuild = $true
            }
        }
        validation = [ordered]@{
            requireExactChromeEngine = $true
            requireOpenVrRuntimeState = $true
            requireOpenVrFocusCycle = $false
            requireOpenVrDashboardCycle = $true
            requireProductionGpuSyncNone = $true
            requirePerformanceSummary = $true
            requireRepeatedPresentation = $true
            requireFlatTheaterUi = $true
            requirePositional6Dof = $true
            requireBodyIk = $true
            requireGameplayInput = $true
        }
        deployment = @(
            [ordered]@{ role = "proxy"; destination = "d3d9.dll"; sha256 = $StereoVerifierProxyHash },
            [ordered]@{ role = "openvr_runtime"; destination = "openvr_api.dll"; sha256 = $StereoVerifierOpenVrHash },
            [ordered]@{ role = "openvr_action_manifest"; destination = "cojvr_openvr_input/actions.json"; sha256 = $StereoVerifierActionManifestHash },
            [ordered]@{ role = "openvr_binding_psvr2_sense"; destination = "cojvr_openvr_input/bindings/psvr2_sense.json"; sha256 = $StereoVerifierSenseBindingHash }
        )
    })
    Write-Utf8Json (Join-Path $StereoVerifierGame ".cojvr-d3d9-stage.json") ([ordered]@{
        runId = $StereoVerifierRunId
        buildManifestId = [string]$StereoBuildManifest.manifestId
        buildManifestSha256 = $StereoVerifierBuildManifestHash
        diagnosticMode = "d3d9_native_stereo"
    })

    $StereoOrientation1 = "camera_probe_event: event=camera_hmd_orientation_applied result=ok pose_sequence=2 yaw_degrees=10 pitch_degrees=0 roll_degrees=0 roll_mode=native_camera_basis relative_head_position=(0.060000,0.000000,0.000000) head_position_valid=true tracked_head_position=(106.0000,200.0000,300.0000) natural_forward=(0.0000,0.2000,0.9800) leveled_forward=(0.0000,0.0000,1.0000) natural_up=(0.0000,0.9800,-0.2000) leveled_up=(0.0000,1.0000,0.0000) natural_determinant=1 applied_determinant=1 native_homogeneous_layout=true source_world_homogeneous_layout=true source_view_homogeneous_layout=true injected_view_homogeneous_layout=true render_basis_observed=true render_basis_changed=true view_matrix_observed=true view_matrix_changed=true projection_matrix_changed=true view_projection_changed=true restore_deferred=true;stereo=true;renderer_camera_match=true"
    $StereoOrientation2 = "camera_probe_event: event=camera_hmd_orientation_applied result=ok pose_sequence=3 yaw_degrees=0 pitch_degrees=5 roll_degrees=6 roll_mode=native_camera_basis relative_head_position=(0.000000,-0.100000,-0.040000) head_position_valid=true tracked_head_position=(50.0000,50.0000,74.0000) natural_forward=(0.1000,-0.1500,0.9800) leveled_forward=(0.1015,0.0000,0.9948) natural_up=(0.0000,0.9880,0.1540) leveled_up=(0.0000,1.0000,0.0000) natural_determinant=1 applied_determinant=1 native_homogeneous_layout=true source_world_homogeneous_layout=true source_view_homogeneous_layout=true injected_view_homogeneous_layout=true render_basis_observed=true render_basis_changed=true view_matrix_observed=true view_matrix_changed=true projection_matrix_changed=true view_projection_changed=true restore_deferred=true;stereo=true;renderer_camera_match=true"
    $StereoFrame1 = "camera_probe_event: event=camera_native_stereo_frame result=ok frame_sequence=1 pose_sequence=2 left_camera_applied=true left_projection_applied=true left_captured=true left_state_restored=true right_rendered=true right_full_view_pass=true right_view_guard_restored=true right_captured=true right_state_restored=true submitted=true transport_accepted=true content_hash_deferred=true left_renderer_camera_match=true right_renderer_camera_match=true left_hash=0 right_hash=0 distinct_eye_content=false left_eye_x=-0.032 right_eye_x=0.032 left_eye_position=(-0.032000,0.000000,0.000000) right_eye_position=(0.032000,0.000000,0.000000) relative_head_position=(0.060000,0.000000,0.000000) head_position_valid=true positional_6dof=true game_units_per_meter=100 left_applied_position=(96.8000,200.0000,300.0000) right_applied_position=(103.2000,200.0000,300.0000) left_fov=-1,0.8,-1,1 right_fov=-0.8,1,-1,1 left_frustum=-2,1.5,-1.8,1.8,1,1000 right_frustum=-1.5,2,-1.8,1.8,1,1000 render_view_rva=0x30fb0 render_core_rva=0x30e00"
    $StereoFrame2 = "camera_probe_event: event=camera_native_stereo_frame result=ok frame_sequence=2 pose_sequence=3 left_camera_applied=true left_projection_applied=true left_captured=true left_state_restored=true right_rendered=true right_full_view_pass=true right_view_guard_restored=true right_captured=true right_state_restored=true submitted=true transport_accepted=true content_hash_deferred=true left_renderer_camera_match=true right_renderer_camera_match=true left_hash=0 right_hash=0 distinct_eye_content=false left_eye_x=-0.032 right_eye_x=0.032 left_eye_position=(-0.032000,0.000000,0.000000) right_eye_position=(0.032000,0.000000,0.000000) relative_head_position=(0.000000,-0.100000,-0.040000) head_position_valid=true positional_6dof=true game_units_per_meter=100 left_applied_position=(46.8000,60.0000,70.0000) right_applied_position=(53.2000,60.0000,70.0000) left_fov=-1,0.8,-1,1 right_fov=-0.8,1,-1,1 left_frustum=-2,1.5,-1.8,1.8,1,1000 right_frustum=-1.5,2,-1.8,1.8,1,1000 render_view_rva=0x30fb0 render_core_rva=0x30e00"
    $StereoTrackingInput = "camera_probe_event: event=body_tracking_input result=observed detail=frame_sequence=2;controller_pose_source=handgrip;raw_role_fallback=false;left_position_valid=true;left_orientation_valid=true;left_position=(-0.200000,1.200000,-0.300000);right_position_valid=true;right_orientation_valid=true;right_position=(0.200000,1.200000,-0.300000)"
    $StereoLeftArm = "camera_probe_event: event=body_arm_tracking result=applied detail=frame_sequence=2;being_generation=1;side=left;controller_target=(10.000000,20.000000,30.000000);shoulder=(1.000000,2.000000,3.000000);elbow=(4.000000,5.000000,6.000000);wrist=(7.000000,8.000000,9.000000);upper_element_position=(1.500000,2.000000,3.000000);upper_element_up=(0.000000,1.000000,0.000000);upper_element_forward=(0.000000,0.000000,1.000000);forearm_element_position=(4.500000,5.000000,6.000000);forearm_element_up=(0.000000,1.000000,0.000000);forearm_element_forward=(0.000000,0.000000,1.000000);elbow_target=(4.000000,5.000000,6.000000);upper_target_position=(1.500000,2.000000,3.000000);upper_target_up=(0.000000,1.000000,0.000000);upper_target_forward=(0.000000,0.000000,1.000000);forearm_target_position=(4.500000,5.000000,6.000000);forearm_target_up=(0.000000,1.000000,0.000000);forearm_target_forward=(0.000000,0.000000,1.000000);upper_rotation_axis=(0.000000,0.000000,1.000000);upper_rotation_degrees=0;upper_rotation_no_op=true;forearm_rotation_axis=(0.000000,0.000000,1.000000);forearm_rotation_degrees=0;forearm_rotation_no_op=true;rotation_plan_valid=true;upper_length=30;lower_length=28;plan_valid=true;target_clamped=false;write_enabled=true;write_allowed=true;write_ok=true;rollback_attempted=false;rollback_ok=true;hand_orientation=natural;basis_source=GetElementPos/GetElementLeftVector/GetElementUpVector;writer=BoneRotate"
    $StereoRightArm = "camera_probe_event: event=body_arm_tracking result=applied detail=frame_sequence=2;being_generation=1;side=right;controller_target=(-10.000000,20.000000,30.000000);shoulder=(-1.000000,2.000000,3.000000);elbow=(-4.000000,5.000000,6.000000);wrist=(-7.000000,8.000000,9.000000);upper_element_position=(-1.500000,2.000000,3.000000);upper_element_up=(0.000000,1.000000,0.000000);upper_element_forward=(0.000000,0.000000,1.000000);forearm_element_position=(-4.500000,5.000000,6.000000);forearm_element_up=(0.000000,1.000000,0.000000);forearm_element_forward=(0.000000,0.000000,1.000000);elbow_target=(-4.000000,5.000000,6.000000);upper_target_position=(-1.500000,2.000000,3.000000);upper_target_up=(0.000000,1.000000,0.000000);upper_target_forward=(0.000000,0.000000,1.000000);forearm_target_position=(-4.500000,5.000000,6.000000);forearm_target_up=(0.000000,1.000000,0.000000);forearm_target_forward=(0.000000,0.000000,1.000000);upper_rotation_axis=(0.000000,0.000000,1.000000);upper_rotation_degrees=0;upper_rotation_no_op=true;forearm_rotation_axis=(0.000000,0.000000,1.000000);forearm_rotation_degrees=0;forearm_rotation_no_op=true;rotation_plan_valid=true;upper_length=30;lower_length=28;plan_valid=true;target_clamped=false;write_enabled=true;write_allowed=true;write_ok=true;rollback_attempted=false;rollback_ok=true;hand_orientation=natural;basis_source=GetElementPos/GetElementLeftVector/GetElementUpVector;writer=BoneRotate"
    $StereoLeftArm = $StereoLeftArm -replace "writer=BoneRotate", "writer=RotateElementWithChildren"
    $StereoRightArm = $StereoRightArm -replace "writer=BoneRotate", "writer=RotateElementWithChildren"
    $StereoLeftArm = $StereoLeftArm -replace ";forearm_element_forward=\(0.000000,0.000000,1.000000\);", ";forearm_element_forward=(0.000000,0.000000,1.000000);foretwist_element_position=(6.000000,7.000000,8.000000);foretwist_element_up=(0.000000,1.000000,0.000000);foretwist_element_forward=(0.000000,0.000000,1.000000);hand_element_position=(7.000000,8.000000,9.000000);hand_element_up=(0.000000,1.000000,0.000000);hand_element_forward=(0.000000,0.000000,1.000000);hand_target_up=(0.000000,1.000000,0.000000);hand_target_forward=(0.000000,0.000000,1.000000);"
    $StereoRightArm = $StereoRightArm -replace ";forearm_element_forward=\(0.000000,0.000000,1.000000\);", ";forearm_element_forward=(0.000000,0.000000,1.000000);foretwist_element_position=(-6.000000,7.000000,8.000000);foretwist_element_up=(0.000000,1.000000,0.000000);foretwist_element_forward=(0.000000,0.000000,1.000000);hand_element_position=(-7.000000,8.000000,9.000000);hand_element_up=(0.000000,1.000000,0.000000);hand_element_forward=(0.000000,0.000000,1.000000);hand_target_up=(0.000000,1.000000,0.000000);hand_target_forward=(0.000000,0.000000,1.000000);"
    $StereoLeftArm = $StereoLeftArm -replace ";rotation_plan_valid=true;", ";upper_native_axis=(0.000000,0.000000,1.000000);forearm_native_axis=(0.000000,0.000000,1.000000);forearm_twist_native_axis=(1.000000,0.000000,0.000000);forearm_twist_degrees=0;forearm_twist_requested_degrees=45;forearm_twist_limit_degrees=0;forearm_twist_limited=true;forearm_twist_no_op=true;twist_owner=foretwist_element;hand_residual_source=post_foretwist_observed_basis;hand_residual_native_axis=(0.000000,0.000000,1.000000);hand_residual_degrees=45;hand_rotation_mode=bounded_hand_residual;hand_rotation_limit_degrees=30;hand_residual_rejected=true;reach_write_safe=true;skinning_contract=foretwist_sibling_swing_plus_bounded_hand_residual;skinning_frames_reached=true;skinning_axis_error=0;hand_native_axis=(0.000000,0.000000,1.000000);hand_rotation_degrees=0;hand_rotation_no_op=true;native_axis_space=element_local;elbow_target_error=0;wrist_target_error=0;targets_reached=true;hand_up_error=0.5;hand_forward_error=0.5;hand_orientation_reached=false;controller_orientation_valid=true;orientation_calibration_recenter_sequence=1;rotation_plan_valid=true;"
    $StereoRightArm = $StereoRightArm -replace ";rotation_plan_valid=true;", ";upper_native_axis=(0.000000,0.000000,1.000000);forearm_native_axis=(0.000000,0.000000,1.000000);forearm_twist_native_axis=(1.000000,0.000000,0.000000);forearm_twist_degrees=0;forearm_twist_requested_degrees=45;forearm_twist_limit_degrees=0;forearm_twist_limited=true;forearm_twist_no_op=true;twist_owner=foretwist_element;hand_residual_source=post_foretwist_observed_basis;hand_residual_native_axis=(0.000000,0.000000,1.000000);hand_residual_degrees=45;hand_rotation_mode=bounded_hand_residual;hand_rotation_limit_degrees=30;hand_residual_rejected=true;reach_write_safe=true;skinning_contract=foretwist_sibling_swing_plus_bounded_hand_residual;skinning_frames_reached=true;skinning_axis_error=0;hand_native_axis=(0.000000,0.000000,1.000000);hand_rotation_degrees=0;hand_rotation_no_op=true;native_axis_space=element_local;elbow_target_error=0;wrist_target_error=0;targets_reached=true;hand_up_error=0.5;hand_forward_error=0.5;hand_orientation_reached=false;controller_orientation_valid=true;orientation_calibration_recenter_sequence=1;rotation_plan_valid=true;"
    $StereoLeftArm = $StereoLeftArm -replace ";upper_length=30;lower_length=28;", ";upper_length=30;lower_length=28;raw_target_distance=50;effective_target_distance=50;reach_adjustment=0;reach_adjusted=false;reach_policy=native_chain_hard_clamp;"
    $StereoRightArm = $StereoRightArm -replace ";upper_length=30;lower_length=28;", ";upper_length=30;lower_length=28;raw_target_distance=50;effective_target_distance=50;reach_adjustment=0;reach_adjusted=false;reach_policy=native_chain_hard_clamp;"
    $StereoLeftArm = $StereoLeftArm -replace ";hand_orientation=natural;", ";tracking_forward=-z_to_negative_native_forward;hand_orientation=calibrated_controller_delta_bounded_hand_residual;"
    $StereoRightArm = $StereoRightArm -replace ";hand_orientation=natural;", ";tracking_forward=-z_to_negative_native_forward;hand_orientation=calibrated_controller_delta_bounded_hand_residual;"
    $StereoLeftArm = $StereoLeftArm -replace ";basis_source=", ";tracking_basis=level_recenter_minus_actor_yaw;basis_source="
    $StereoRightArm = $StereoRightArm -replace ";basis_source=", ";tracking_basis=level_recenter_minus_actor_yaw;basis_source="
    $StereoLeftArmNaturalProbe = "camera_probe_event: event=body_arm_write_probe result=natural detail=frame_sequence=2;side=left;phase=before_write;writer=RotateElementWithChildren"
    $StereoRightArmNaturalProbe = "camera_probe_event: event=body_arm_write_probe result=natural detail=frame_sequence=2;side=right;phase=before_write;writer=RotateElementWithChildren"
    $StereoLeftArmWriteProbe = "camera_probe_event: event=body_arm_write_probe result=changed detail=frame_sequence=2;side=left;phase=after_write;expects_change=true;changed_from_natural=true;targets_reached=true;elbow_target_error=0;wrist_target_error=0;hand_orientation_reached=false;hand_up_error=0.5;hand_forward_error=0.5;writer=RotateElementWithChildren"
    $StereoRightArmWriteProbe = "camera_probe_event: event=body_arm_write_probe result=changed detail=frame_sequence=2;side=right;phase=after_write;expects_change=true;changed_from_natural=true;targets_reached=true;elbow_target_error=0;wrist_target_error=0;hand_orientation_reached=false;hand_up_error=0.5;hand_forward_error=0.5;writer=RotateElementWithChildren"
    $StereoLeftArmRenderLeft = "camera_probe_event: event=body_arm_render_probe result=changed detail=frame_sequence=2;side=left;phase=left_eye_complete;changed_from_natural=true;post_write_geometry_valid=true;matches_post_write=true;writer=RotateElementWithChildren"
    $StereoLeftArmRenderRight = "camera_probe_event: event=body_arm_render_probe result=changed detail=frame_sequence=2;side=left;phase=right_eye_complete;changed_from_natural=true;post_write_geometry_valid=true;matches_post_write=true;writer=RotateElementWithChildren"
    $StereoRightArmRenderLeft = "camera_probe_event: event=body_arm_render_probe result=changed detail=frame_sequence=2;side=right;phase=left_eye_complete;changed_from_natural=true;post_write_geometry_valid=true;matches_post_write=true;writer=RotateElementWithChildren"
    $StereoRightArmRenderRight = "camera_probe_event: event=body_arm_render_probe result=changed detail=frame_sequence=2;side=right;phase=right_eye_complete;changed_from_natural=true;post_write_geometry_valid=true;matches_post_write=true;writer=RotateElementWithChildren"
    $StereoLeftArmRestore = "camera_probe_event: event=body_arm_restore result=ok detail=frame_sequence=2;side=left;hand_restored=true;forearm_twist_restored=true;forearm_restored=true;upper_restored=true;geometry_read=true;geometry_restored=true;writer=RotateElementWithChildren;twist_owner=foretwist_element;transaction=post_stereo_capture;inverse_geometry_restored=true;exact_restore_attempted=false;exact_restore_ok=false;restore_joint_error=0.00390625;restore_element_position_error=0.00390625;restore_axis_error=0.0002"
    $StereoRightArmRestore = "camera_probe_event: event=body_arm_restore result=ok detail=frame_sequence=2;side=right;hand_restored=true;forearm_twist_restored=true;forearm_restored=true;upper_restored=true;geometry_read=true;geometry_restored=true;writer=RotateElementWithChildren;twist_owner=foretwist_element;transaction=post_stereo_capture;inverse_geometry_restored=true;exact_restore_attempted=false;exact_restore_ok=false;restore_joint_error=0.00390625;restore_element_position_error=0.00390625;restore_axis_error=0.0002"
    $StereoLeftLeg = "camera_probe_event: event=body_lower_tracking result=observed detail=frame_sequence=2;being_generation=1;side=left;actor_position=(0.000000,0.000000,0.000000);pelvis_offset=(0.000000,90.000000,0.000000);pelvis_target=(0.000000,90.000000,0.000000);hip=(-10.000000,90.000000,0.000000);knee=(-10.000000,50.000000,5.000000);ankle=(-10.000000,10.000000,0.000000);foot_target=(-10.000000,10.000000,0.000000);thigh_length=40.3;shin_length=40.3;plan_valid=true;target_clamped=false;knee_plane_valid=true;write_enabled=false;foot_orientation=natural;basis_source=GetBoneDirVector/GetBonePerpVector;writer=disabled_preflight"
    $StereoRightLeg = "camera_probe_event: event=body_lower_tracking result=observed detail=frame_sequence=2;being_generation=1;side=right;actor_position=(0.000000,0.000000,0.000000);pelvis_offset=(0.000000,90.000000,0.000000);pelvis_target=(0.000000,90.000000,0.000000);hip=(10.000000,90.000000,0.000000);knee=(10.000000,50.000000,5.000000);ankle=(10.000000,10.000000,0.000000);foot_target=(10.000000,10.000000,0.000000);thigh_length=40.3;shin_length=40.3;plan_valid=true;target_clamped=false;knee_plane_valid=true;write_enabled=false;foot_orientation=natural;basis_source=GetBoneDirVector/GetBonePerpVector;writer=disabled_preflight"
    $StereoVerifierLog = @(
        "run_start: run_id=$StereoVerifierRunId build_manifest_id=$($StereoBuildManifest.manifestId) pid=789",
        "native_stereo_presenter: status=started owner_thread=openvr+d3d11 mode=flat_theater_to_native_stereo",
        "native_stereo_runtime: status=started backend=openvr owner=presenter_thread recommended_eye=2000x2040 left_eye_x=-0.032 right_eye_x=0.032 pose_semantics=eye_to_head",
        "openvr_gpu_handoff: native_stereo=D3D9Ex_shared_texture+CopyResource;flat_fallback=UpdateSubresource;producer_sync=nonblocking_D3D9_event_query;consumer_sync=nonblocking_D3D11_event_query;submit=Submit_TextureWithPose;handoff=PostPresentHandoff",
        "openvr_scene_state: phase=initialized;process_id=789;scene_focus_process_id=0;can_render_scene=false;input_available=true;dashboard_visible=true;should_pause=false;should_reduce_rendering_work=false",
        "openvr_runtime_state: phase=initialized;lifecycle=ready;initialized=true;connected=true;focused=false;tracking_valid=false;presenting=false;shutdown_requested=false",
        "native_stereo_presenter_transition: status=content_mode mode=flat_theater",
        "flat_ui_pointer: status=hit;hand=right;pose=tip_with_grip_fallback;u=0.5000;v=0.5000;pixel=960,540;beam_origin=820,700;source=1920x1080;select=false;smoothing=0.40;visual=cyan_beam_reticle;route=flat_theater_menu_pointer",
        "camera_probe_event: event=flat_ui_pointer result=active detail=active=true;hand=right;source=1920x1080;route=win32_cursor_position+SendInput_absolute+WM_MOUSEMOVE",
        "camera_probe_event: event=flat_ui_select result=applied detail=source=right_r2;pointer_active=true;current_ui_is_loading=false;route=GameWithMenu.sm_cIntroModule.OnInputKey",
        "camera_probe_event: event=flat_ui_select result=applied detail=source=right_cross;pointer_active=true;current_ui_is_loading=false;route=GameWithMenu.sm_cMenuModule.GetCurrentUI.Enter",
        "camera_probe_event: event=flat_ui_back result=applied detail=source=right_circle;route=GameWithMenu.sm_cMenuModule.GetCurrentUI.CallOnInputKeyGlobal(Escape)",
        "camera_probe_event: event=flat_ui_select result=applied detail=source=right_r2;pointer_active=true;current_ui_is_loading=false;route=LawmanGame.sm_cActiveGameModule.cMenu.GetCurrentUI.Enter",
        "native_stereo_presenter_transition: status=content_mode mode=native_stereo",
        "native_stereo_d3d9_factory: status=d3d9ex_primary fallback=classic_create_device transport=d3d9ex_shared_texture_ring",
        "native_stereo_factory_hook: status=installed",
        "native_stereo_device: status=observed device=0x1234 generation=1 device_api=d3d9ex",
        "camera_probe_bootstrap: status=installed system_d3d9=expected",
        "camera_probe_event: event=camera_probe_install result=installed pose_source=none native_stereo=available",
        "camera_probe_event: event=camera_probe_control_loaded result=accepted detail=generation=2;enabled=false;fov=natural;yaw=0;pitch=0;tracking_enabled=true;body_ik_enabled=true;recenter=false",
        "camera_probe_event: event=subtitle_state result=observed detail=frame_sequence=2;subtitles_enabled=true;dialog_playing=true;current_line_visible=true;dialog_subtitle_visible=true;diagnostic=java_visible",
        "camera_probe_event: event=blocking_ui_select result=applied detail=source=right_r2;game_timer_valid=true;game_timer_frozen=true;current_ui_is_loading=true;gameplay_suppressed=true;body_mutation_suppressed=true;fire_suppressed_until_release=true;route=GameUILoading.OnInputKey",
        "camera_probe_event: event=blocking_ui_resume result=observed detail=game_timer_valid=true;game_timer_frozen=false;trigger_released=true;route=LawmanModule.TimerStart",
        $StereoTrackingInput,
        $StereoLeftArmNaturalProbe,
        $StereoLeftArmWriteProbe,
        $StereoLeftArm,
        $StereoRightArmNaturalProbe,
        $StereoRightArmWriteProbe,
        $StereoRightArm,
        $StereoLeftArmRenderLeft,
        $StereoRightArmRenderLeft,
        $StereoLeftArmRenderRight,
        $StereoRightArmRenderRight,
        $StereoLeftArmRestore,
        $StereoRightArmRestore,
        $StereoLeftLeg,
        $StereoRightLeg,
        "camera_probe_event: event=camera_probe_control_loaded result=accepted generation=1 tracking_enabled=true",
        "camera_probe_event: event=camera_hmd_recentered result=ok generation=1 pose_sequence=1 stereo=true",
        "native_stereo_capture: status=source eye=left transport=d3d9ex_shared_texture_ring;capture_source=render_target0;source_is_backbuffer=true;eye_surface=2560x1440;viewport=0,0,2560,1440,0,1;format=21;source_pool=0;source_msaa=0;capture_pool=default;capture_usage=render_target;capture_msaa=0;ring_slots=3;ring_depth=1;gpu_copy_queue_ms=0.150",
        "native_stereo_capture: status=source eye=right transport=d3d9ex_shared_texture_ring;capture_source=render_target0;source_is_backbuffer=true;eye_surface=2560x1440;viewport=0,0,2560,1440,0,1;format=21;source_pool=0;source_msaa=0;capture_pool=default;capture_usage=render_target;capture_msaa=0;ring_slots=3;ring_depth=1;gpu_copy_queue_ms=0.160",
        $StereoOrientation1,
        $StereoFrame1,
        $StereoOrientation2,
        $StereoFrame2,
        "native_stereo_capture_timing: status=ok frame_sequence=2 eye=left transport=d3d9ex_shared_texture_ring;capture_source=render_target0;source_is_backbuffer=true;eye_surface=2560x1440;viewport=0,0,2560,1440,0,1;format=21;source_pool=0;source_msaa=0;capture_pool=default;capture_usage=render_target;capture_msaa=0;ring_slots=3;ring_depth=1;gpu_copy_queue_ms=0.150",
        "native_stereo_capture_timing: status=ok frame_sequence=2 eye=right transport=d3d9ex_shared_texture_ring;capture_source=render_target0;source_is_backbuffer=true;eye_surface=2560x1440;viewport=0,0,2560,1440,0,1;format=21;source_pool=0;source_msaa=0;capture_pool=default;capture_usage=render_target;capture_msaa=0;ring_slots=3;ring_depth=1;gpu_copy_queue_ms=0.160",
        "native_stereo_producer_timing: status=published transport=d3d9ex_shared_texture_ring;frame_sequence=2;render_pose_sequence=3;generation=1;eye_surface=2560x1440;format=21;shared_handles_distinct=true;ring_depth=1;ring_depth_peak=2;fence_poll_ms=0.010;producer_wait_ms=0.000;deferred_readback_ms=0.000;cpu_copy_ms=0.000;producer_collect_ms=0.020",
        "native_stereo_presenter_frame: status=new frame_sequence=1;render_pose_sequence=2;generation=1;presentation_mode=native_stereo;transport=d3d9ex_shared_texture_ring;left_hash=0;right_hash=0;distinct_eye_content=not_cpu_sampled;eye_resources_distinct=true;distinct_check=separate_shared_eye_resources;hash_mode=disabled_gpu_resident;hash_ms=0.000;upload_ms=0.200;shared_open_ms=0.050;consumer_gpu_copy_queue_ms=0.150;consumer_wait_ms=0.000;consumer_pending_fences=1;frame_age_ms=2.500",
        "native_stereo_presenter_frame: status=new frame_sequence=2;render_pose_sequence=3;generation=1;presentation_mode=native_stereo;transport=d3d9ex_shared_texture_ring;left_hash=0;right_hash=0;distinct_eye_content=not_cpu_sampled;eye_resources_distinct=true;distinct_check=separate_shared_eye_resources;hash_mode=disabled_gpu_resident;hash_ms=0.000;upload_ms=0.190;shared_open_ms=0.040;consumer_gpu_copy_queue_ms=0.140;consumer_wait_ms=0.000;consumer_pending_fences=1;frame_age_ms=2.400",
        "native_stereo_presenter_timing: status=ok submit_sequence=2;capture_sequence=2;render_pose_sequence=3;pose_mode=explicit_render_pose;content=new;left_result=0;right_result=0;wait_pose_ms=5.000;submit_ms=0.400",
        "openvr_runtime_state: phase=transition;lifecycle=ready;initialized=true;connected=true;focused=true;tracking_valid=true;presenting=true;shutdown_requested=false",
        "openvr_scene_state: phase=first_submit;process_id=789;scene_focus_process_id=789;can_render_scene=true;input_available=true;dashboard_visible=false;should_pause=false;should_reduce_rendering_work=false",
        "openvr_scene_state: phase=dashboard_opened;process_id=789;scene_focus_process_id=789;can_render_scene=true;input_available=true;dashboard_visible=true;should_pause=true;should_reduce_rendering_work=true",
        "openvr_scene_state: phase=dashboard_closed;process_id=789;scene_focus_process_id=789;can_render_scene=true;input_available=true;dashboard_visible=false;should_pause=false;should_reduce_rendering_work=false",
        "native_stereo_presenter_timing: status=ok submit_sequence=3;capture_sequence=2;render_pose_sequence=3;pose_mode=explicit_render_pose;content=repeated;left_result=0;right_result=0;wait_pose_ms=5.100;submit_ms=0.300",
        "openvr_input: status=started action_sets=/actions/global,/actions/gameplay recenter=/actions/global/in/recenter hand_pose=/user/hand/{left,right}/pose/handgrip aim_pose=/user/hand/{left,right}/pose/tip gameplay=semantic_sense_profile owner=presenter_thread",
        "openvr_controller_pose: source=handgrip;left_active=true;right_active=true;raw_role_fallback=false",
        "camera_probe_event: event=body_player_reconciliation result=camera_only detail=frame_sequence=2;being_generation=1;recentered=false;write_needed=false;write_ok=true;actor_write=false;player_before=(1,2,3);player_desired=(1,2,3);tracking_offset=(0.06,0,0);world_offset=(0,0,0);render_head_position=(0.06,0,0);game_units_per_meter=100;mode=camera_only_collision_safe;collision_owner=native_actor",
        "camera_probe_event: event=body_visual_roomscale result=applied detail=frame_sequence=2;being_generation=1;pelvis_element=10;tracking_offset=(0.060000,0.000000,0.000000);world_offset=(6.000000,0.000000,0.000000);actor_write=false;scope=stereo_render_only;restore=after_both_eyes",
        "camera_probe_event: event=body_visual_roomscale_restore result=ok detail=frame_sequence=2;being_generation=1;pelvis_element=10;world_offset=(6.000000,0.000000,0.000000);reason=stereo_complete",
        "camera_probe_event: event=body_yaw_tracking result=applied detail=frame_sequence=2;being_generation=1;camera_compensation_degrees=0;actor_delta_degrees=-10;actor_target_degrees=-10;route=PlayerBeing.RotateHorizontally;commit_after_stereo_restore=true",
        "camera_probe_event: event=controller_aim result=applied detail=frame_sequence=2;pose_source=tip;left_orientation_valid=true;right_orientation_valid=true;left_direction=(0,0,-1);right_direction=(0,0,-1);left_origin_valid=true;right_origin_valid=true;left_origin=(1,2,3);right_origin=(-1,2,3);left_visual_origin_written=true;right_visual_origin_written=true;left_hand_index=1;right_hand_index=0;write_boundary=post_game_update_pre_render;tracking_basis=level_recenter_minus_actor_yaw;direction_owner=m_avLookDirDevForHand;fire_origin=controller_tip_attack_transition;visual_origin_owner=m_avAimFromPoint;fire_origin_release=native_UpdateLookAndAimPoints;native_accuracy_spread=preserved;network_forced_branch=unused",
        "camera_probe_event: event=controller_aim_geometry result=observed detail=frame_sequence=2;left_valid=true;left_grip_to_tip_distance_m=0.10;left_grip_to_tip_direction=(0,0,-1);left_dot_pos_x=0;left_dot_neg_x=0;left_dot_pos_y=0;left_dot_neg_y=0;left_dot_pos_z=-1;left_dot_neg_z=1;right_valid=true;right_grip_to_tip_distance_m=0.10;right_grip_to_tip_direction=(0,0,-1);right_dot_pos_x=0;right_dot_neg_x=0;right_dot_pos_y=0;right_dot_neg_y=0;right_dot_pos_z=-1;right_dot_neg_z=1;axis_reference=grip_to_tip_tracking_space;aim_axis_assumption=negative_z",
        "camera_probe_event: event=controller_aim_fire_transition result=applied detail=frame_sequence=2;side=left;hand_index=1;gameplay_translate_applied=true;direction_valid=true;direction=(0,0,-1);origin_valid=true;origin=(1,2,3);being_look_from_write_succeeded=true;translate_succeeded=true;retained_for_attack=true;published_fire_origin=(1,2,3);tip_geometry_valid=true;grip_to_tip_distance_m=0.10;grip_to_tip_direction=(0,0,-1);dot_pos_x=0;dot_neg_x=0;dot_pos_y=0;dot_neg_y=0;dot_pos_z=-1;dot_neg_z=1;aim_axis_assumption=negative_z;direction_owner=m_avLookDirDevForHand;fire_origin_owner=Being.m_vLookFromPoint;attack_order=OnBeingsFrame.UpdateHandStates_before_OnPostBeingsUpdate.UpdateLookAndAimPoints",
        "camera_probe_event: event=gameplay_input result=applied detail=frame_sequence=2;active=true;move=0.7,0.8;turn=0.4,0;fire_left=true;fire_right=false;jump=false;reload=false;run=false;crouch=false;physical_crouch=true;interact=false;weapon_next=false;weapon_previous=false;kick=false;blocking_ui_gameplay_suppressed=false;locomotion_policy=coj_inputanalog_per_axis_deadzone_0.04;route=GameInputController.InputAction.Translate;analog_transaction=LockApplyControllerState+dispatch+UnlockApplyControllerState+ApplyControllerState;",
        "camera_probe_event: event=native_stereo_ui_back result=applied detail=frame_sequence=2;source=right_circle;route=LawmanGame.sm_cActiveGameModule.OnInputKey(Escape)",
        "openvr_input_event: action=recenter result=pressed source=global_action owner=presenter_thread",
        "camera_probe_event: event=camera_hmd_recenter_requested result=ok detail=source=openvr_global_action;pose_sequence=3",
        "camera_probe_event: event=body_arm_recovery result=ok detail=frame_sequence=3;recenter_sequence=3;previous_fault=false;transaction_active=false;natural_verified=true;calibration_preserved=true",
        "camera_probe_event: event=camera_probe_control_loaded result=accepted generation=2 tracking_enabled=false",
        "camera_probe_event: event=camera_probe_passthrough result=disabled",
        "camera_probe_event: event=camera_probe_restore result=restored camera_restored_slots=2;view_restored_slots=1",
        "native_stereo_factory_hook: status=restored",
        "native_stereo_shutdown: stage=presenter_begin",
        "native_stereo_presenter_shutdown: stage=d3d11_begin",
        "native_stereo_presenter_shutdown: stage=d3d11_end",
        "native_stereo_presenter_shutdown: stage=runtime_begin",
        "openvr_runtime_state: phase=shutdown_complete;lifecycle=shutdown_complete;initialized=false;connected=false;focused=false;tracking_valid=false;presenting=false;shutdown_requested=false",
        "native_stereo_presenter_shutdown: stage=runtime_end",
        "native_stereo_presenter: status=stopped",
        "native_stereo_presenter_stop: shutdown_complete=true",
        "native_stereo_shutdown: stage=presenter_end",
        "native_stereo_shutdown: stage=capture_begin",
        "native_stereo_shutdown: stage=capture_end",
        "native_stereo_gpu_transport_summary: frames_copied=2;resources_opened=4;copy_fences_completed=2;copy_fence_poll_pending=1;pending_fences_peak=2;open_failures=0;copy_failures=0;abandoned_on_shutdown=0;last_copy_completion_ms=0.500;max_copy_completion_ms=0.700",
        "native_stereo_transport_summary: frames_fenced=3;frames_collected=2;capture_ring_drops=0;capture_query_not_ready=1;capture_query_flushes=1;shared_frames_published=2;cpu_fallback_frames=0;fallback_activations=0;consumer_releases=2;capture_ring_depth=0;capture_ring_depth_peak=2;mailbox_published=2;mailbox_replaced=0;frames_uploaded=2;new_submissions=2;repeat_submissions=4;shared_frames_copied=2;shared_resources_opened=4;shared_copy_fences_completed=2;shared_pending_copy_fences=0;shared_pending_copy_fences_peak=2;shared_open_failures=0;shared_copy_failures=0;submit_failures=0",
        "native_stereo_runtime: status=stopped",
        "run_end: run_id=$StereoVerifierRunId"
    )
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog
    & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
        -GameDirectory $StereoVerifierGame | Out-Null

    $TransportRun = Get-Content -LiteralPath (Join-Path $StereoVerifierGame ".cojvr-run.json") -Raw | ConvertFrom-Json
    $TransportRun.validation | Add-Member -NotePropertyName profile -NotePropertyValue "transport" -Force
    $TransportRun.validation | Add-Member -NotePropertyName requireOpenVrDashboardCycle -NotePropertyValue $false -Force
    $TransportRun.validation | Add-Member -NotePropertyName requireFlatTheaterUi -NotePropertyValue $false -Force
    $TransportRun.validation | Add-Member -NotePropertyName requireExplicitPassthroughDisable -NotePropertyValue $false -Force
    $TransportRun.validation | Add-Member -NotePropertyName requirePositional6Dof -NotePropertyValue $false -Force
    $TransportRun.validation | Add-Member -NotePropertyName requireBodyIk -NotePropertyValue $false -Force
    $TransportRun.validation | Add-Member -NotePropertyName requireGameplayInput -NotePropertyValue $false -Force
    $TransportRun.validation | Add-Member -NotePropertyName requireSubtitleState -NotePropertyValue $false -Force
    $TransportRun.validation | Add-Member -NotePropertyName requireRecenter -NotePropertyValue $false -Force
    $TransportRun.validation | Add-Member -NotePropertyName requireStereoGeometry -NotePropertyValue $false -Force
    Write-Utf8Json (Join-Path $StereoVerifierGame ".cojvr-run.json") $TransportRun
    $TransportOnlyLog = @($StereoVerifierLog | Where-Object {
        $_ -notmatch "event=subtitle_state" -and
        $_ -notmatch "event=camera_hmd_recentered" -and
        $_ -notmatch "action=recenter" -and
        $_ -notmatch "event=camera_hmd_recenter_requested" -and
        $_ -notmatch "event=camera_native_stereo_frame" -and
        $_ -notmatch "event=camera_hmd_orientation_applied" -and
        $_ -notmatch "phase=dashboard_opened" -and
        $_ -notmatch "phase=dashboard_closed" -and
        $_ -notmatch "event=flat_ui_" -and
        $_ -notmatch "event=blocking_ui_" -and
        $_ -notmatch "event=body_" -and
        $_ -notmatch "event=controller_aim" -and
        $_ -notmatch "event=gameplay_input" -and
        $_ -notmatch "event=native_stereo_ui_back"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $TransportOnlyLog
    & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
        -GameDirectory $StereoVerifierGame | Out-Null

    $TransportRun.validation.profile = "startup"
    $TransportRun.validation | Add-Member -NotePropertyName requireHmdTracking -NotePropertyValue $false -Force
    Write-Utf8Json (Join-Path $StereoVerifierGame ".cojvr-run.json") $TransportRun
    $StartupLog = @(
        "run_start: run_id=$StereoVerifierRunId build_manifest_id=$($StereoBuildManifest.manifestId) pid=789",
        "native_stereo_d3d9_factory: status=d3d9ex_primary fallback=classic_create_device transport=d3d9ex_shared_texture_ring",
        "native_stereo_device: status=observed device=0x1234 generation=1 device_api=d3d9ex",
        "native_stereo_startup_event: event=factory_identity;thread_id=1;factory=0x12;recovered=0x12;device=0x1234;hr=0x0;matches=true",
        "native_stereo_startup_event: event=present_exit;thread_id=1;device=0x1234;generation=1;frame_sequence=90;hr=0x0",
        "native_stereo_flat_theater: status=captured transport=immediate_flat_d3d9_cpu_mailbox",
        "native_stereo_presenter_transition: status=content_mode mode=flat_theater",
        "native_stereo_startup_event: event=device_method_enter;thread_id=1;object=0x1234;hr=0x0;method=Present",
        "native_stereo_startup_event: event=device_method_exit;thread_id=1;object=0x1234;hr=0x0;method=Present",
        "native_stereo_factory_hook: status=restored",
        "native_stereo_device_hook: status=restored",
        "native_stereo_swapchain_hook: status=restored",
        "native_stereo_runtime: status=stopped",
        "run_end: run_id=$StereoVerifierRunId"
    )
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StartupLog
    & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
        -GameDirectory $StereoVerifierGame | Out-Null
    $StartupLogWithoutSustainedPresent = @($StartupLog | Where-Object {
        $_ -notmatch "frame_sequence=90"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StartupLogWithoutSustainedPresent
    $StartupRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $StartupRejected = $true
    }
    Assert-True $StartupRejected "Startup validation accepted a run with no sustained D3D9Ex Present."

    $TransportRun.validation.profile = "full"
    $TransportRun.validation.requireOpenVrDashboardCycle = $true
    $TransportRun.validation.requireFlatTheaterUi = $true
    $TransportRun.validation.requireExplicitPassthroughDisable = $true
    $TransportRun.validation.requirePositional6Dof = $true
    $TransportRun.validation.requireBodyIk = $true
    $TransportRun.validation.requireGameplayInput = $true
    $TransportRun.validation.requireSubtitleState = $true
    $TransportRun.validation.requireRecenter = $true
    $TransportRun.validation.requireStereoGeometry = $true
    Write-Utf8Json (Join-Path $StereoVerifierGame ".cojvr-run.json") $TransportRun
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierDualPointerOwner = @($StereoVerifierLog) +
        "camera_probe_event: event=flat_ui_pointer_game_route result=applied detail=route=MainMenuModule.GetGlobalCursor.UICursor.SetPos+OnMouseMove"
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierDualPointerOwner
    $DualPointerOwnerRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $DualPointerOwnerRejected = $true
    }
    Assert-True $DualPointerOwnerRejected `
        "Native-stereo verifier accepted simultaneous Win32 and logical-Java flat-menu pointer ownership."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierFailedBodyRestore = @($StereoVerifierLog) +
        "camera_probe_event: event=body_visual_roomscale_restore result=failed detail=frame_sequence=3;being_generation=1;pelvis_element=10;world_offset=(6.000000,0.000000,0.000000);reason=stereo_complete;detail=synthetic_failure"
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierFailedBodyRestore
    $FailedBodyRestoreRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $FailedBodyRestoreRejected = $true
    }
    Assert-True $FailedBodyRestoreRejected `
        "Native-stereo verifier accepted a failed room-scale pelvis restoration."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierVerticalPelvisOffset = @($StereoVerifierLog | ForEach-Object {
        if ($_ -match "event=body_visual_roomscale result=applied") {
            $_ -replace "world_offset=\(6\.000000,0\.000000,0\.000000\)",
                "world_offset=(6.000000,-12.000000,0.000000)"
        } else {
            $_
        }
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierVerticalPelvisOffset
    $VerticalPelvisOffsetRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $VerticalPelvisOffsetRejected = $true
    }
    Assert-True $VerticalPelvisOffsetRejected `
        "Native-stereo verifier accepted vertical HMD translation as a visual pelvis room-scale offset."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierExtremeArmDelta = @($StereoVerifierLog | ForEach-Object {
        if ($_ -match "event=body_arm_tracking result=applied .*;side=right;") {
            $_ -replace "forearm_rotation_degrees=0;", "forearm_rotation_degrees=150;"
        } else {
            $_
        }
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierExtremeArmDelta
    $ExtremeArmDeltaRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $ExtremeArmDeltaRejected = $true
    }
    Assert-True $ExtremeArmDeltaRejected `
        "Native-stereo verifier accepted a 150-degree forearm takeover as a valid tracked arm write."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierMissingAnalogTransaction = @($StereoVerifierLog | ForEach-Object {
        $_ -replace ";analog_transaction=LockApplyControllerState\+dispatch\+UnlockApplyControllerState\+ApplyControllerState", ""
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierMissingAnalogTransaction
    $MissingAnalogTransactionRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $MissingAnalogTransactionRejected = $true
    }
    Assert-True $MissingAnalogTransactionRejected `
        "Native-stereo verifier accepted locomotion without the shipped controller-state transaction."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierNoControllerBeamOrigin = @($StereoVerifierLog | ForEach-Object {
        $_ -replace ";beam_origin=820,700;", ";beam_origin=invalid;"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierNoControllerBeamOrigin
    $MissingControllerBeamOriginRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $MissingControllerBeamOriginRejected = $true
    }
    Assert-True $MissingControllerBeamOriginRejected `
        "Native-stereo verifier accepted a flat-theater beam without a projected controller origin."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierNoCrossAccept = @($StereoVerifierLog | Where-Object {
        $_ -notmatch "event=flat_ui_select result=applied detail=source=right_cross;"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierNoCrossAccept
    $MissingCrossAcceptRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $MissingCrossAcceptRejected = $true
    }
    Assert-True $MissingCrossAcceptRejected `
        "Native-stereo verifier accepted flat UI without Cross accept."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierNoCircleBack = @($StereoVerifierLog | Where-Object {
        $_ -notmatch "event=flat_ui_back result=applied detail=source=right_circle;"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierNoCircleBack
    $MissingCircleBackRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $MissingCircleBackRejected = $true
    }
    Assert-True $MissingCircleBackRejected `
        "Native-stereo verifier accepted flat UI without Circle back."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierNoPhysicalCrouch = @($StereoVerifierLog | ForEach-Object {
        $_ -replace ";physical_crouch=true;", ";physical_crouch=false;"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierNoPhysicalCrouch
    $MissingPhysicalCrouchRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $MissingPhysicalCrouchRejected = $true
    }
    Assert-True $MissingPhysicalCrouchRejected `
        "Native-stereo verifier accepted gameplay without a physical crouch sample."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierAppliedAfterRollback = @($StereoVerifierLog | ForEach-Object {
        $_ -replace "rollback_attempted=false;rollback_ok=true", "rollback_attempted=true;rollback_ok=true"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierAppliedAfterRollback
    $AppliedAfterRollbackRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $AppliedAfterRollbackRejected = $true
    }
    Assert-True $AppliedAfterRollbackRejected `
        "Native-stereo verifier accepted an applied arm sample that required rollback."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierOldForwardMapping = @($StereoVerifierLog | ForEach-Object {
        $_ -replace `
            "tracking_forward=-z_to_negative_native_forward", `
            "tracking_forward=-z_to_positive_native_forward"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierOldForwardMapping
    $OldForwardMappingRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $OldForwardMappingRejected = $true
    }
    Assert-True $OldForwardMappingRejected `
        "Native-stereo verifier accepted the physically rejected arm front/back mapping."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierNaturalHandOrientation = @($StereoVerifierLog | ForEach-Object {
        $_ -replace `
            "hand_orientation=calibrated_controller_delta_bounded_hand_residual", `
            "hand_orientation=natural"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierNaturalHandOrientation
    $NaturalHandOrientationRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $NaturalHandOrientationRejected = $true
    }
    Assert-True $NaturalHandOrientationRejected `
        "Native-stereo verifier accepted the old natural-only hand orientation contract."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierWrongTwistOwner = @($StereoVerifierLog | ForEach-Object {
        $_ -replace "twist_owner=foretwist_element", "twist_owner=forearm_element"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierWrongTwistOwner
    $WrongTwistOwnerRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $WrongTwistOwnerRejected = $true
    }
    Assert-True $WrongTwistOwnerRejected `
        "Native-stereo verifier accepted controller twist on the forearm instead of EBones FORETWIST."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierWrongHandResidualSource = @($StereoVerifierLog | ForEach-Object {
        $_ -replace `
            "hand_residual_source=post_foretwist_observed_basis", `
            "hand_residual_source=precomputed_ideal_basis"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierWrongHandResidualSource
    $WrongHandResidualSourceRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $WrongHandResidualSourceRejected = $true
    }
    Assert-True $WrongHandResidualSourceRejected `
        "Native-stereo verifier accepted a hand residual that was not recomputed from the observed post-FORETWIST basis."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierUnboundedHandResidual = @($StereoVerifierLog | ForEach-Object {
        $_ -replace "hand_rotation_degrees=0;hand_rotation_no_op=true", "hand_rotation_degrees=45;hand_rotation_no_op=false"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierUnboundedHandResidual
    $UnboundedHandResidualRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $UnboundedHandResidualRejected = $true
    }
    Assert-True $UnboundedHandResidualRejected `
        "Native-stereo verifier accepted a hand residual above the 30-degree safety bound."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierNonZeroHandRotation = @($StereoVerifierLog | ForEach-Object {
        $_ -replace "skinning_frames_reached=true", `
            "skinning_frames_reached=false"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierNonZeroHandRotation
    $NonZeroHandRotationRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $NonZeroHandRotationRejected = $true
    }
    Assert-True $NonZeroHandRotationRejected `
        "Native-stereo verifier accepted inconsistent sibling skinning frames."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierNeutralGameplay = @($StereoVerifierLog | ForEach-Object {
        if ($_ -match "event=gameplay_input result=applied") {
            "camera_probe_event: event=gameplay_input result=applied detail=frame_sequence=2;active=true;move=0,0;turn=0,0;fire_left=false;fire_right=false;jump=false;reload=false;run=false;crouch=false;interact=false;weapon_next=false;weapon_previous=false;kick=false;route=GameInputController.InputAction.Translate"
        } else {
            $_
        }
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierNeutralGameplay
    $NeutralGameplayRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $NeutralGameplayRejected = $true
    }
    Assert-True $NeutralGameplayRejected `
        "Native-stereo verifier accepted a gameplay-input gate with no non-neutral Sense action."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierNonMutatingWriter = @($StereoVerifierLog | ForEach-Object {
        $_ -replace `
            "event=body_arm_write_probe result=changed detail=frame_sequence=2;side=left;phase=after_write;expects_change=true;changed_from_natural=true", `
            "event=body_arm_write_probe result=natural detail=frame_sequence=2;side=left;phase=after_write;expects_change=true;changed_from_natural=false"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierNonMutatingWriter
    $NonMutatingWriterRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $NonMutatingWriterRejected = $true
    }
    Assert-True $NonMutatingWriterRejected `
        "Native-stereo verifier accepted a non-mutating arm writer."

    $StereoVerifierBadRestore = @($StereoVerifierLog | ForEach-Object {
        if ($_ -match "event=body_arm_restore result=ok .*;side=left;") {
            $_ -replace "geometry_restored=true", "geometry_restored=false"
        } else {
            $_
        }
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierBadRestore
    $BadRestoreRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $BadRestoreRejected = $true
    }
    Assert-True $BadRestoreRejected `
        "Native-stereo verifier accepted an arm restore that did not return to natural geometry."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoVerifierMissingHandRestore = @($StereoVerifierLog | ForEach-Object {
        if ($_ -match "event=body_arm_restore result=ok .*;side=left;") {
            $_ -replace "hand_restored=true", "hand_restored=false"
        } else {
            $_
        }
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierMissingHandRestore
    $MissingHandRestoreRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $MissingHandRestoreRejected = $true
    }
    Assert-True $MissingHandRestoreRejected `
        "Native-stereo verifier accepted a restore that omitted the hand element."
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog

    $StereoSummaryPath = Join-Path $StereoVerifierGame "cojvr-native-stereo-summary.json"
    & (Join-Path $SourceDirectory "tools\summarize_native_stereo_run.ps1") `
        -GameDirectory $StereoVerifierGame `
        -OutputPath $StereoSummaryPath | Out-Null
    $StereoSummary = Get-Content -LiteralPath $StereoSummaryPath -Raw | ConvertFrom-Json
    Assert-True ([string]$StereoSummary.runId -eq $StereoVerifierRunId) `
        "Native-stereo summary was not bound to the current run."
    Assert-True ([int]$StereoSummary.metricsMs.cpuCopy.count -eq 1 -and
        [double]$StereoSummary.metricsMs.cpuCopy.p50 -eq 0.0 -and
        [double]$StereoSummary.metricsMs.producerWait.p50 -eq 0.0 -and
        [double]$StereoSummary.metricsMs.consumerWait.p50 -eq 0.0) `
        "Native-stereo summary did not preserve zero-wait GPU transport timing."
    Assert-True ([double]$StereoSummary.metricsMs.consumerGpuCopyQueue.p50 -gt 0.0 -and
        [double]$StereoSummary.metricsMs.frameAge.p50 -gt 0.0) `
        "Native-stereo summary did not preserve GPU-copy or frame-age timing."
    Assert-True ([int]$StereoSummary.motion.headTranslationMetres.count -eq 2 -and
        [double]$StereoSummary.motion.headTranslationMetres.max -gt 0.10) `
        "Native-stereo summary did not preserve physical head-translation evidence."
    Assert-True (-not [bool]$StereoSummary.runtime.focusCycleObserved) `
        "Native-stereo summary conflated the dashboard cycle with a scene-focus cycle."
    Assert-True ([bool]$StereoSummary.runtime.dashboardCycleObserved) `
        "Native-stereo summary did not recognize the dashboard cycle."
    Assert-True ([bool]$StereoSummary.runtime.shutdownCompleteObserved) `
        "Native-stereo summary did not recognize shutdown completion."
    Assert-True ([uint64]$StereoSummary.transport.framesCollected -eq 2 -and
        [uint64]$StereoSummary.transport.sharedFramesPublished -eq 2 -and
        [uint64]$StereoSummary.transport.cpuFallbackFrames -eq 0 -and
        [uint64]$StereoSummary.transport.sharedFramesCopied -eq 2 -and
        [uint64]$StereoSummary.transport.sharedCopyFailures -eq 0) `
        "Native-stereo summary did not preserve GPU-resident transport counters."

    $StereoPackagePath = Join-Path $TestRoot "$StereoVerifierRunId.zip"
    & (Join-Path $SourceDirectory "tools\collect_run_evidence.ps1") `
        -GameDirectory $StereoVerifierGame `
        -OutputPath $StereoPackagePath | Out-Null
    $StereoEvidenceManifest = Get-Content -LiteralPath `
        (Join-Path $StereoVerifierRunDirectory "evidence-manifest.json") -Raw | ConvertFrom-Json
    Assert-True ([bool]$StereoEvidenceManifest.analysis.nativeStereoSummaryRequired -and
        [bool]$StereoEvidenceManifest.analysis.nativeStereoSummaryCollected) `
        "Required native-stereo summary was not bound into the run evidence manifest."
    Assert-True (Test-Path -LiteralPath `
        (Join-Path $StereoVerifierRunDirectory "analysis\native-stereo-summary.json") -PathType Leaf) `
        "Native-stereo summary was not copied into the run evidence package."

    $StereoVerifierNoDashboardCycle = @($StereoVerifierLog | Where-Object {
        $_ -notmatch "^openvr_scene_state: phase=dashboard_opened;"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierNoDashboardCycle
    $MissingDashboardCycleRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $MissingDashboardCycleRejected = $true
    }
    Assert-True $MissingDashboardCycleRejected `
        "Native-stereo verifier accepted a run without the required SteamVR dashboard cycle."

    $StereoVerifierNoGpuHandoff = @($StereoVerifierLog | Where-Object {
        $_ -notmatch "^openvr_gpu_handoff:"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierNoGpuHandoff
    $MissingGpuHandoffRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $MissingGpuHandoffRejected = $true
    }
    Assert-True $MissingGpuHandoffRejected `
        "Native-stereo verifier accepted a run without the bound production GPU handoff policy."

    $StereoVerifierNoRepeatedPresentation = @($StereoVerifierLog | ForEach-Object {
        $_ -replace "repeat_submissions=4", "repeat_submissions=0"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierNoRepeatedPresentation
    $MissingRepeatedPresentationRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $MissingRepeatedPresentationRejected = $true
    }
    Assert-True $MissingRepeatedPresentationRejected `
        "Native-stereo verifier accepted a run without repeated-frame presentation."

    $StereoVerifierNoDashboardResume = @($StereoVerifierLog | Where-Object {
        $_ -notmatch "submit_sequence=3;"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierNoDashboardResume
    $MissingDashboardResumeRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $MissingDashboardResumeRejected = $true
    }
    Assert-True $MissingDashboardResumeRejected `
        "Native-stereo verifier accepted a dashboard close without resumed presentation."

    $StereoVerifierNoShutdownComplete = @($StereoVerifierLog | Where-Object {
        $_ -notmatch "^openvr_runtime_state: phase=shutdown_complete;" -and
        $_ -notmatch "^native_stereo_presenter_stop: shutdown_complete=true"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierNoShutdownComplete
    $MissingShutdownCompleteRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $MissingShutdownCompleteRejected = $true
    }
    Assert-True $MissingShutdownCompleteRejected `
        "Native-stereo verifier accepted a run without the final OpenVR shutdown-complete state."

    $StereoVerifierNoPosition = @($StereoVerifierLog | ForEach-Object {
        $_ `
            -replace "head_position_valid=true", "head_position_valid=false" `
            -replace "relative_head_position=\([-+0-9.,]+\)", "relative_head_position=(0.000000,0.000000,0.000000)"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierNoPosition
    $MissingPositionRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $MissingPositionRejected = $true
    }
    Assert-True $MissingPositionRejected `
        "Native-stereo verifier accepted a run without valid physical 6DOF translation."

    $StereoVerifierNoRoll = @($StereoVerifierLog | ForEach-Object {
        $_ -replace "roll_degrees=6", "roll_degrees=0"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierNoRoll
    $MissingRollRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $MissingRollRejected = $true
    }
    Assert-True $MissingRollRejected `
        "Native-stereo verifier accepted a run without a meaningful native-camera roll sample."

    $StereoVerifierResidualRestore = @($StereoVerifierLog | ForEach-Object {
        $_ -replace "restore_axis_error=0.0002", "restore_axis_error=0.01"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierResidualRestore
    $ResidualRestoreRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $ResidualRestoreRejected = $true
    }
    Assert-True $ResidualRestoreRejected `
        "Native-stereo verifier accepted an arm restore with a visible residual axis error."

    $StereoVerifierFlat = @($StereoVerifierLog | ForEach-Object {
        $_ -replace "eye_resources_distinct=true", "eye_resources_distinct=false"
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierFlat
    $FlatStereoRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $FlatStereoRejected = $true
    }
    Assert-True $FlatStereoRejected `
        "Native-stereo verifier accepted aliased/non-distinct left/right GPU eye resources."

    $StereoVerifierWrongCamera = @($StereoVerifierLog | ForEach-Object {
        if ($_ -match "camera_native_stereo_frame result=ok frame_sequence=1") {
            $_ -replace "right_renderer_camera_match=true", "right_renderer_camera_match=false"
        } else { $_ }
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierWrongCamera
    $WrongCameraRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $WrongCameraRejected = $true
    }
    Assert-True $WrongCameraRejected `
        "Native-stereo verifier accepted an eye pass without renderer-camera correlation."

    $StereoVerifierCoreOnly = @($StereoVerifierLog | ForEach-Object {
        if ($_ -match "camera_native_stereo_frame result=ok frame_sequence=1") {
            $_ -replace "right_full_view_pass=true", "right_full_view_pass=false"
        } else { $_ }
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierCoreOnly
    $CoreOnlyRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $CoreOnlyRejected = $true
    }
    Assert-True $CoreOnlyRejected `
        "Native-stereo verifier accepted a right eye that skipped the complete render-view wrapper."

    $StereoVerifierWrongScale = @($StereoVerifierLog | ForEach-Object {
        if ($_ -match "camera_native_stereo_frame result=ok frame_sequence=1") {
            $_ `
                -replace "game_units_per_meter=100", "game_units_per_meter=1" `
                -replace "left_applied_position=\(96.8000,200.0000,300.0000\)", "left_applied_position=(99.9680,200.0000,300.0000)" `
                -replace "right_applied_position=\(103.2000,200.0000,300.0000\)", "right_applied_position=(100.0320,200.0000,300.0000)"
        } else { $_ }
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierWrongScale
    $WrongScaleRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $WrongScaleRejected = $true
    }
    Assert-True $WrongScaleRejected `
        "Native-stereo verifier accepted the obsolete 1:1 metre-to-game-unit eye scale."

    $StereoVerifierMissingViewport = @($StereoVerifierLog | ForEach-Object {
        if ($_ -match "native_stereo_capture: status=source eye=left") {
            $_ -replace "viewport=0,0,2560,1440,0,1", "viewport=unavailable"
        } else { $_ }
    })
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierMissingViewport
    $MissingViewportRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $MissingViewportRejected = $true
    }
    Assert-True $MissingViewportRejected `
        "Native-stereo verifier accepted a run without a left-eye capture viewport."

    Write-Host "PASS - provenance manifests, camera/HMD/stereo controls and verifier guards"
} finally {
    if (Test-Path -LiteralPath $ResolvedTestRoot -PathType Container) {
        Remove-Item -LiteralPath $ResolvedTestRoot -Recurse -Force
    }
}
