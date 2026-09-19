param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,

    [string]$ProxyPath = "",
    [string]$BuildManifestPath = "",
    [string]$RunId = "",

    [ValidateSet("full", "performance")]
    [string]$ValidationProfile = "full"
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "deployment_transaction.ps1")

function Get-PeMachine([string]$Path) {
    $Stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    try {
        $Reader = [System.IO.BinaryReader]::new($Stream)
        try {
            if ($Stream.Length -lt 64 -or $Reader.ReadUInt16() -ne 0x5A4D) {
                throw "'$Path' is not a valid PE image (missing MZ header)."
            }
            $Stream.Position = 0x3C
            $PeOffset = $Reader.ReadUInt32()
            if ($PeOffset -gt ($Stream.Length - 6)) {
                throw "'$Path' has an invalid PE header offset."
            }
            $Stream.Position = $PeOffset
            if ($Reader.ReadUInt32() -ne 0x00004550) {
                throw "'$Path' is not a valid PE image (missing PE signature)."
            }
            return $Reader.ReadUInt16()
        } finally {
            $Reader.Dispose()
        }
    } finally {
        $Stream.Dispose()
    }
}

function Assert-Win32Pe([string]$Path, [string]$Role) {
    $Machine = Get-PeMachine $Path
    if ($Machine -ne 0x014C) {
        throw "$Role '$Path' is not Win32/x86 PE (machine=0x$($Machine.ToString('X4')))."
    }
}

if ([string]::IsNullOrWhiteSpace($ProxyPath)) {
    $ProxyPath = Join-Path $PSScriptRoot "..\build\win32-debug\Release\d3d9.dll"
}

$ExpectedCoJHash = "5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE"
$ExpectedChromeEngineHash = "DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8"
$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$ProxyPath = [System.IO.Path]::GetFullPath($ProxyPath)
$Executable = Join-Path $GameDirectory "CoJ.exe"
$ChromeEngine = Join-Path $GameDirectory "ChromeEngine3.dll"
$Destination = Join-Path $GameDirectory "d3d9.dll"
$Backup = Join-Path $GameDirectory "d3d9.cojvr-backup.dll"
$Log = Join-Path $GameDirectory "cojvr.log"
$State = Join-Path $GameDirectory ".cojvr-d3d9-stage.json"
$CurrentRun = Join-Path $GameDirectory ".cojvr-run.json"
$EvidenceRoot = Join-Path $GameDirectory ".cojvr-evidence"
$D3D9ExMarker = Join-Path $GameDirectory ".cojvr-d3d9-ex-bridge"
$CameraControl = Join-Path $GameDirectory "cojvr-camera-control.json"
$CameraControlBackup = Join-Path $GameDirectory "cojvr-camera-control.cojvr-backup.json"
$NativeStereoSummary = Join-Path $GameDirectory "cojvr-native-stereo-summary.json"
$OpenVrDestination = Join-Path $GameDirectory "openvr_api.dll"
$OpenVrBackup = Join-Path $GameDirectory "openvr_api.cojvr-backup.dll"
$OpenVrInputDestination = Join-Path $GameDirectory "cojvr_openvr_input"
$OpenVrInputBackup = Join-Path $GameDirectory "cojvr_openvr_input.cojvr-backup"
$ProxyTemporary = Join-Path $GameDirectory "d3d9.cojvr-installing.dll"
$CameraControlTemporary = Join-Path $GameDirectory "cojvr-camera-control.cojvr-installing.json"
$OpenVrTemporary = Join-Path $GameDirectory "openvr_api.cojvr-installing.dll"
$OpenVrInputTemporary = Join-Path $GameDirectory "cojvr_openvr_input.cojvr-installing"
$TransactionJournal = Join-Path $GameDirectory ".cojvr-deployment-transaction.json"

if (Get-Process -Name CoJ -ErrorAction SilentlyContinue) {
    throw "Call of Juarez is running. Close it before staging a candidate."
}
if (Test-Path -LiteralPath $TransactionJournal -PathType Leaf) {
    [void](Complete-CojvrDeploymentRecovery $GameDirectory)
    Write-Host "Recovered the previous interrupted CoJ VR deployment transaction."
}

$ProxyLeaf = [System.IO.Path]::GetFileName($ProxyPath)
$IsReadbackDiagnostic = $ProxyLeaf -ieq "d3d9_readback.dll"
$IsOpenVrFlatDiagnostic = $ProxyLeaf -ieq "d3d9_openvr_flat.dll"
$IsCameraProbe = $ProxyLeaf -ieq "d3d9_camera_probe.dll"
$IsHmdCamera = $ProxyLeaf -ieq "d3d9_hmd_camera.dll"
$IsNativeStereo = $ProxyLeaf -ieq "d3d9_native_stereo.dll"
$IsCameraIntegration = $IsCameraProbe -or $IsHmdCamera -or $IsNativeStereo
$IsOpenVrIntegration = $IsHmdCamera -or $IsNativeStereo
$DiagnosticMode = if ($IsReadbackDiagnostic) {
    "d3d9_readback"
} elseif ($IsOpenVrFlatDiagnostic) {
    "d3d9_openvr_flat"
} elseif ($IsCameraProbe) {
    "d3d9_camera_probe"
} elseif ($IsHmdCamera) {
    "d3d9_hmd_camera"
} elseif ($IsNativeStereo) {
    "d3d9_native_stereo"
} else {
    "d3d9_forwarding"
}

$D3D9ExEnvironmentRequested = $env:COJVR_D3D9_EX_BRIDGE -match "^(1|true|on)$"
if ($D3D9ExEnvironmentRequested -or (Test-Path -LiteralPath $D3D9ExMarker -PathType Leaf)) {
    throw "D3D9Ex is a laboratory-only path and must be disabled before classic diagnostic staging. Remove the marker/unset COJVR_D3D9_EX_BRIDGE explicitly."
}

if ([string]::IsNullOrWhiteSpace($BuildManifestPath)) {
    $BuildManifestPath = Join-Path `
        ([System.IO.Path]::GetDirectoryName($ProxyPath)) `
        "$([System.IO.Path]::GetFileNameWithoutExtension($ProxyPath)).build-manifest.json"
}
$BuildManifestPath = [System.IO.Path]::GetFullPath($BuildManifestPath)

if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "CoJ.exe was not found in '$GameDirectory'."
}

if (-not (Test-Path -LiteralPath $ProxyPath -PathType Leaf)) {
    throw "Release proxy was not found at '$ProxyPath'. Build the Release preset first."
}

if (-not (Test-Path -LiteralPath $BuildManifestPath -PathType Leaf)) {
    throw "Build manifest was not found at '$BuildManifestPath'. Run tools\new_build_manifest.ps1 for this exact proxy first."
}

$BuildManifest = Get-Content -LiteralPath $BuildManifestPath -Raw | ConvertFrom-Json
if ([string]$BuildManifest.manifestType -ne "cojvr-build" -or
    [int]$BuildManifest.schemaVersion -ne 1) {
    throw "Build manifest '$BuildManifestPath' has an unsupported schema."
}
if ([string]$BuildManifest.build.platform -ne "Win32") {
    throw "Build manifest platform '$($BuildManifest.build.platform)' is not the required Win32 target."
}
if ([string]$BuildManifest.build.diagnosticMode -ne $DiagnosticMode) {
    throw "Build manifest diagnostic mode '$($BuildManifest.build.diagnosticMode)' does not match '$DiagnosticMode'."
}
$ProxyArtifact = @($BuildManifest.artifacts | Where-Object { [string]$_.role -eq "proxy" })
if ($ProxyArtifact.Count -ne 1) {
    throw "Build manifest must contain exactly one proxy artifact."
}
$ProxyHash = (Get-FileHash -LiteralPath $ProxyPath -Algorithm SHA256).Hash.ToUpperInvariant()
if ($ProxyHash -ne ([string]$ProxyArtifact[0].sha256).ToUpperInvariant()) {
    throw "Proxy SHA-256 does not match the selected build manifest."
}
$OpenVrArtifact = @($BuildManifest.artifacts | Where-Object { [string]$_.role -eq "openvr_runtime" })
$OpenVrSource = $null
$OpenVrActionManifestArtifact = @($BuildManifest.artifacts | Where-Object { [string]$_.role -eq "openvr_action_manifest" })
$OpenVrSenseBindingArtifact = @($BuildManifest.artifacts | Where-Object { [string]$_.role -eq "openvr_binding_psvr2_sense" })
$OpenVrActionManifestSource = $null
$OpenVrSenseBindingSource = $null
if ($IsOpenVrIntegration) {
    if ($OpenVrArtifact.Count -ne 1) {
        throw "OpenVR camera/stereo manifest must contain exactly one OpenVR runtime artifact."
    }
    $OpenVrSource = Join-Path ([System.IO.Path]::GetDirectoryName($ProxyPath)) ([string]$OpenVrArtifact[0].fileName)
    if (-not (Test-Path -LiteralPath $OpenVrSource -PathType Leaf)) {
        throw "OpenVR runtime artifact was not found at '$OpenVrSource'."
    }
    $OpenVrHash = (Get-FileHash -LiteralPath $OpenVrSource -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($OpenVrHash -ne ([string]$OpenVrArtifact[0].sha256).ToUpperInvariant()) {
        throw "OpenVR runtime SHA-256 does not match the selected build manifest."
    }
}
if ($IsNativeStereo) {
    if ($OpenVrActionManifestArtifact.Count -ne 1 -or $OpenVrSenseBindingArtifact.Count -ne 1) {
        throw "Native-stereo manifest must contain one OpenVR action manifest and one PS VR2 Sense binding artifact."
    }
    $ProxyDirectory = [System.IO.Path]::GetDirectoryName($ProxyPath)
    $OpenVrActionManifestSource = Join-Path $ProxyDirectory ([string]$OpenVrActionManifestArtifact[0].fileName)
    $OpenVrSenseBindingSource = Join-Path $ProxyDirectory ([string]$OpenVrSenseBindingArtifact[0].fileName)
    foreach ($Pair in @(
        @($OpenVrActionManifestSource, [string]$OpenVrActionManifestArtifact[0].sha256, "action manifest"),
        @($OpenVrSenseBindingSource, [string]$OpenVrSenseBindingArtifact[0].sha256, "PS VR2 Sense binding")
    )) {
        if (-not (Test-Path -LiteralPath $Pair[0] -PathType Leaf)) {
            throw "OpenVR $($Pair[2]) artifact was not found at '$($Pair[0])'."
        }
        $InputHash = (Get-FileHash -LiteralPath $Pair[0] -Algorithm SHA256).Hash.ToUpperInvariant()
        if ($InputHash -ne $Pair[1].ToUpperInvariant()) {
            throw "OpenVR $($Pair[2]) SHA-256 does not match the selected build manifest."
        }
    }
}

$ActualHash = (Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash.ToUpperInvariant()
if ($ActualHash -ne $ExpectedCoJHash) {
    throw "CoJ.exe SHA-256 is not the known exact build. Expected $ExpectedCoJHash, got $ActualHash."
}
Assert-Win32Pe $Executable "Game executable"
Assert-Win32Pe $ProxyPath "Proxy artifact"
if ($IsOpenVrIntegration) {
    Assert-Win32Pe $OpenVrSource "OpenVR runtime artifact"
}

$ChromeEngineHash = if (Test-Path -LiteralPath $ChromeEngine -PathType Leaf) {
    (Get-FileHash -LiteralPath $ChromeEngine -Algorithm SHA256).Hash.ToUpperInvariant()
} else {
    $null
}

if ($IsCameraIntegration -and $ChromeEngineHash -ne $ExpectedChromeEngineHash) {
    throw "Camera probe requires the exact inspected ChromeEngine3.dll. Expected $ExpectedChromeEngineHash, got '$ChromeEngineHash'."
}
if ($IsCameraIntegration) {
    Assert-Win32Pe $ChromeEngine "ChromeEngine3.dll"
}

if (Test-Path -LiteralPath $Backup) {
    throw "Backup '$Backup' already exists. Restore or remove it before staging another proxy."
}

if (Test-Path -LiteralPath $State) {
    throw "A previous CoJ VR staging state already exists at '$State'. Unstage it first."
}
if ($IsCameraIntegration -and (Test-Path -LiteralPath $CameraControlBackup)) {
    throw "Camera-control backup '$CameraControlBackup' already exists. Restore or remove it before staging the camera probe."
}
if ($IsOpenVrIntegration -and (Test-Path -LiteralPath $OpenVrBackup)) {
    throw "OpenVR backup '$OpenVrBackup' already exists. Restore or remove it before staging the OpenVR candidate."
}
if ($IsNativeStereo -and (Test-Path -LiteralPath $OpenVrInputBackup)) {
    throw "OpenVR input backup '$OpenVrInputBackup' already exists. Restore or remove it before staging the native-stereo candidate."
}
foreach ($TemporaryPath in @($ProxyTemporary, $CameraControlTemporary, $OpenVrTemporary, $OpenVrInputTemporary)) {
    if (Test-Path -LiteralPath $TemporaryPath) {
        throw "Temporary deployment path '$TemporaryPath' already exists without a recovery journal. Refusing to overwrite it."
    }
}

if ([string]::IsNullOrWhiteSpace($RunId)) {
    $RunId = "{0}-{1}" -f [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssZ"), ([Guid]::NewGuid().ToString("N").Substring(0, 12))
}
if ($RunId -notmatch "^[A-Za-z0-9._-]+$") {
    throw "RunId may contain only letters, digits, '.', '_' and '-'."
}

$RunDirectory = Join-Path (Join-Path $EvidenceRoot "runs") $RunId
if (Test-Path -LiteralPath $RunDirectory) {
    throw "Run evidence directory '$RunDirectory' already exists. Choose a unique RunId."
}
$HadOriginal = Test-Path -LiteralPath $Destination -PathType Leaf
$HadOriginalCameraControl = $IsCameraIntegration -and (Test-Path -LiteralPath $CameraControl -PathType Leaf)
$HadOriginalOpenVr = $IsOpenVrIntegration -and (Test-Path -LiteralPath $OpenVrDestination -PathType Leaf)
$HadOriginalOpenVrInput = $IsNativeStereo -and (Test-Path -LiteralPath $OpenVrInputDestination -PathType Container)
$CameraControlText = "{`n  `"enabled`": false,`n  `"trackingEnabled`": false,`n  `"bodyIkEnabled`": false,`n  `"recenter`": false,`n  `"yawDegrees`": 0,`n  `"pitchDegrees`": 0`n}`n"
$CameraControlBytes = [System.Text.UTF8Encoding]::new($false).GetBytes($CameraControlText)
$CameraControlHashBytes = [System.Security.Cryptography.SHA256]::Create().ComputeHash($CameraControlBytes)
$CameraControlStagedHash = -join ($CameraControlHashBytes | ForEach-Object { $_.ToString("X2") })
$JournalAssets = @(
    [ordered]@{
        role = "proxy"; kind = "file"; destination = "d3d9.dll"; backup = "d3d9.cojvr-backup.dll"
        temporary = "d3d9.cojvr-installing.dll"
        hadOriginal = $HadOriginal
        originalSha256 = if ($HadOriginal) { Get-CojvrFileSha256 $Destination } else { $null }
        stagedSha256 = $ProxyHash
    }
)
if ($IsCameraIntegration) {
    $JournalAssets += [ordered]@{
        role = "camera_control"; kind = "file"; destination = "cojvr-camera-control.json"; backup = "cojvr-camera-control.cojvr-backup.json"
        temporary = "cojvr-camera-control.cojvr-installing.json"
        hadOriginal = $HadOriginalCameraControl
        originalSha256 = if ($HadOriginalCameraControl) { Get-CojvrFileSha256 $CameraControl } else { $null }
        stagedSha256 = $CameraControlStagedHash
    }
}
if ($IsOpenVrIntegration) {
    $JournalAssets += [ordered]@{
        role = "openvr_runtime"; kind = "file"; destination = "openvr_api.dll"; backup = "openvr_api.cojvr-backup.dll"
        temporary = "openvr_api.cojvr-installing.dll"
        hadOriginal = $HadOriginalOpenVr
        originalSha256 = if ($HadOriginalOpenVr) { Get-CojvrFileSha256 $OpenVrDestination } else { $null }
        stagedSha256 = $OpenVrHash
    }
}
if ($IsNativeStereo) {
    $OriginalInputManifest = if ($HadOriginalOpenVrInput) {
        @(Get-CojvrDirectoryManifest $OpenVrInputDestination)
    } else { @() }
    $JournalAssets += [ordered]@{
        role = "openvr_input"; kind = "directory"; destination = "cojvr_openvr_input"; backup = "cojvr_openvr_input.cojvr-backup"
        temporary = "cojvr_openvr_input.cojvr-installing"
        hadOriginal = $HadOriginalOpenVrInput
        originalManifest = @($OriginalInputManifest)
        stagedManifest = @(
            [ordered]@{ path = "actions.json"; sha256 = ([string]$OpenVrActionManifestArtifact[0].sha256).ToUpperInvariant() },
            [ordered]@{ path = "bindings/psvr2_sense.json"; sha256 = ([string]$OpenVrSenseBindingArtifact[0].sha256).ToUpperInvariant() }
        )
    }
}
[void](Write-CojvrDeploymentJournal $GameDirectory "stage" $RunId $DiagnosticMode $JournalAssets)

try {
$HistoricalDirectory = Join-Path `
    (Join-Path $EvidenceRoot "historical") `
    ("{0}-{1}" -f [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssZ"), ([Guid]::NewGuid().ToString("N").Substring(0, 8)))
New-Item -ItemType Directory -Path $RunDirectory -Force | Out-Null
Copy-Item -LiteralPath $BuildManifestPath -Destination (Join-Path $RunDirectory "build-manifest.json")

$HistoricalFiles = @(
    $Log,
    (Join-Path $GameDirectory "callstack.txt"),
    $CurrentRun,
    $NativeStereoSummary
)
foreach ($HistoricalFile in $HistoricalFiles) {
    if (Test-Path -LiteralPath $HistoricalFile -PathType Leaf) {
        New-Item -ItemType Directory -Path $HistoricalDirectory -Force | Out-Null
        Move-Item -LiteralPath $HistoricalFile -Destination `
            (Join-Path $HistoricalDirectory ([System.IO.Path]::GetFileName($HistoricalFile)))
    }
}

if ($HadOriginal) {
    Move-Item -LiteralPath $Destination -Destination $Backup
    Invoke-CojvrDeploymentCheckpoint "stage_proxy_backup_created"
    Write-Host "Backed up existing d3d9.dll to d3d9.cojvr-backup.dll"
}

    Install-CojvrVerifiedFile `
        $ProxyPath $Destination $ProxyTemporary $ProxyHash "stage_proxy_published"
    if ($IsOpenVrIntegration) {
        if ($HadOriginalOpenVr) {
            Move-Item -LiteralPath $OpenVrDestination -Destination $OpenVrBackup
            Invoke-CojvrDeploymentCheckpoint "stage_openvr_backup_created"
        }
        Install-CojvrVerifiedFile `
            $OpenVrSource $OpenVrDestination $OpenVrTemporary $OpenVrHash `
            "stage_openvr_published"
    }
    if ($IsNativeStereo) {
        if ($HadOriginalOpenVrInput) {
            Move-Item -LiteralPath $OpenVrInputDestination -Destination $OpenVrInputBackup
            Invoke-CojvrDeploymentCheckpoint "stage_openvr_input_backup_created"
        }
        New-Item -ItemType Directory -Path (Join-Path $OpenVrInputTemporary "bindings") -Force | Out-Null
        Invoke-CojvrDeploymentCheckpoint "stage_openvr_input_temporary_created"
        Copy-Item -LiteralPath $OpenVrActionManifestSource -Destination (Join-Path $OpenVrInputTemporary "actions.json")
        Invoke-CojvrDeploymentCheckpoint "stage_openvr_action_copied"
        Copy-Item -LiteralPath $OpenVrSenseBindingSource -Destination (Join-Path $OpenVrInputTemporary "bindings\psvr2_sense.json")
        Invoke-CojvrDeploymentCheckpoint "stage_openvr_binding_copied"
        $OpenVrInputAssets = @(
            $JournalAssets | Where-Object { [string]$_.role -eq "openvr_input" }
        )
        if ($OpenVrInputAssets.Count -ne 1) {
            throw "Deployment journal does not contain exactly one OpenVR input asset."
        }
        $ExpectedInputManifest = @($OpenVrInputAssets[0].stagedManifest)
        if (-not (Test-CojvrDirectoryMatchesManifest $OpenVrInputTemporary $ExpectedInputManifest)) {
            throw "Temporary OpenVR input deployment failed manifest verification."
        }
        Move-Item -LiteralPath $OpenVrInputTemporary -Destination $OpenVrInputDestination
        Invoke-CojvrDeploymentCheckpoint "stage_openvr_input_published"
        if (-not (Test-CojvrDirectoryMatchesManifest $OpenVrInputDestination $ExpectedInputManifest)) {
            throw "Installed OpenVR input deployment failed manifest verification."
        }
    }
    if ($IsCameraIntegration) {
        if ($HadOriginalCameraControl) {
            Move-Item -LiteralPath $CameraControl -Destination $CameraControlBackup
            Invoke-CojvrDeploymentCheckpoint "stage_camera_control_backup_created"
        }
        Install-CojvrVerifiedBytes `
            $CameraControlBytes $CameraControl $CameraControlTemporary $CameraControlStagedHash `
            "stage_camera_control_published"
    }
    @{
        schemaVersion = 1
        hadOriginalD3D9 = $HadOriginal
        runId = $RunId
        diagnosticMode = $DiagnosticMode
        buildManifestId = [string]$BuildManifest.manifestId
        buildManifestSha256 = (Get-FileHash -LiteralPath $BuildManifestPath -Algorithm SHA256).Hash.ToUpperInvariant()
        stagedProxySha256 = (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash.ToUpperInvariant()
        cameraControlManaged = $IsCameraIntegration
        hadOriginalCameraControl = $HadOriginalCameraControl
        openVrRuntimeManaged = $IsOpenVrIntegration
        hadOriginalOpenVr = $HadOriginalOpenVr
        stagedOpenVrSha256 = if ($IsOpenVrIntegration) {
            (Get-FileHash -LiteralPath $OpenVrDestination -Algorithm SHA256).Hash.ToUpperInvariant()
        } else { $null }
        openVrInputManaged = $IsNativeStereo
        hadOriginalOpenVrInput = $HadOriginalOpenVrInput
        stagedOpenVrActionManifestSha256 = if ($IsNativeStereo) {
            (Get-FileHash -LiteralPath (Join-Path $OpenVrInputDestination "actions.json") -Algorithm SHA256).Hash.ToUpperInvariant()
        } else { $null }
        stagedOpenVrSenseBindingSha256 = if ($IsNativeStereo) {
            (Get-FileHash -LiteralPath (Join-Path $OpenVrInputDestination "bindings\psvr2_sense.json") -Algorithm SHA256).Hash.ToUpperInvariant()
        } else { $null }
    } | ConvertTo-Json | Set-Content -LiteralPath $State -Encoding UTF8
    Invoke-CojvrDeploymentCheckpoint "stage_state_written"

    $Deployment = @(
        [ordered]@{
            role = "proxy"
            destination = "d3d9.dll"
            sha256 = (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash.ToUpperInvariant()
        }
    )
    if ($IsOpenVrIntegration) {
        $Deployment += [ordered]@{
            role = "openvr_runtime"
            destination = "openvr_api.dll"
            sha256 = (Get-FileHash -LiteralPath $OpenVrDestination -Algorithm SHA256).Hash.ToUpperInvariant()
        }
    }
    if ($IsNativeStereo) {
        $Deployment += [ordered]@{
            role = "openvr_action_manifest"
            destination = "cojvr_openvr_input/actions.json"
            sha256 = (Get-FileHash -LiteralPath (Join-Path $OpenVrInputDestination "actions.json") -Algorithm SHA256).Hash.ToUpperInvariant()
        }
        $Deployment += [ordered]@{
            role = "openvr_binding_psvr2_sense"
            destination = "cojvr_openvr_input/bindings/psvr2_sense.json"
            sha256 = (Get-FileHash -LiteralPath (Join-Path $OpenVrInputDestination "bindings\psvr2_sense.json") -Algorithm SHA256).Hash.ToUpperInvariant()
        }
    }

    $RequireBodyValidation = $IsNativeStereo -and $ValidationProfile -eq "full"
    $RunManifest = [ordered]@{
        schemaVersion = 1
        manifestType = "cojvr-run"
        runId = $RunId
        createdUtc = [DateTime]::UtcNow.ToString("o")
        status = "staged"
        diagnosticMode = $DiagnosticMode
        buildManifestId = [string]$BuildManifest.manifestId
        buildManifestSha256 = (Get-FileHash -LiteralPath $BuildManifestPath -Algorithm SHA256).Hash.ToUpperInvariant()
        source = $BuildManifest.source
        game = [ordered]@{
            executable = [ordered]@{
                fileName = "CoJ.exe"
                sha256 = $ActualHash
                knownExactBuild = $ActualHash -eq $ExpectedCoJHash
            }
            engine = [ordered]@{
                fileName = "ChromeEngine3.dll"
                sha256 = $ChromeEngineHash
                knownInspectedBuild = $ChromeEngineHash -eq $ExpectedChromeEngineHash
            }
        }
        validation = [ordered]@{
            profile = if ($IsNativeStereo) { $ValidationProfile } else { "default" }
            requireExactChromeEngine = $IsCameraIntegration
            requireOpenVrRuntimeState = $IsNativeStereo
            requireOpenVrFocusCycle = $false
            # The full/body profile is intentionally isolated from the SteamVR
            # dashboard. Dashboard focus is a presentation/lifecycle gate and
            # has already been exercised independently; requiring it during a
            # body-composition run can make the scene non-interactive and
            # contaminate the IK evidence. Keep it on the performance profile.
            requireOpenVrDashboardCycle = $IsNativeStereo -and $ValidationProfile -eq "performance"
            requireProductionGpuSyncNone = $IsNativeStereo
            requirePerformanceSummary = $IsNativeStereo
            requireRepeatedPresentation = $IsNativeStereo
            requireFlatTheaterUi = $IsNativeStereo
            requirePositional6Dof = $RequireBodyValidation
            requireBodyIk = $RequireBodyValidation
            requireGameplayInput = $RequireBodyValidation
        }
        deployment = @($Deployment)
    }
    $RunJson = $RunManifest | ConvertTo-Json -Depth 10
    [System.IO.File]::WriteAllText(
        $CurrentRun,
        $RunJson + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))
    Invoke-CojvrDeploymentCheckpoint "stage_run_manifest_written"
    Copy-Item -LiteralPath $CurrentRun -Destination (Join-Path $RunDirectory "run-manifest.json") -Force
    Invoke-CojvrDeploymentCheckpoint "stage_evidence_manifest_written"
    Remove-Item -LiteralPath $TransactionJournal -Force
} catch {
    if (Test-Path -LiteralPath $CurrentRun) {
        Remove-Item -LiteralPath $CurrentRun -Force
    }
    if (Test-Path -LiteralPath $TransactionJournal -PathType Leaf) {
        [void](Complete-CojvrDeploymentRecovery $GameDirectory)
    }
    throw
}

if ($IsReadbackDiagnostic) {
    Write-Host "Staged CoJ VR classic-D3D9 readback diagnostic proxy."
} elseif ($IsOpenVrFlatDiagnostic) {
    Write-Host "Staged CoJ VR EndScene-driven OpenVR flat diagnostic proxy."
} elseif ($IsCameraProbe) {
    Write-Host "Staged exact-build ChromeEngine3 camera-control probe with D3D9 forwarding only."
} elseif ($IsHmdCamera) {
    Write-Host "Staged exact-build ChromeEngine3 HMD camera candidate with OpenVR pose input and D3D9 forwarding only."
} elseif ($IsNativeStereo) {
    Write-Host "Staged exact-build ChromeEngine3 native-stereo candidate with OpenVR pose/optics, PS VR2 Sense recenter and two engine view passes."
} else {
    Write-Host "Staged CoJ VR D3D9 forwarding proxy with Present/Reset observation hooks."
}
Write-Host "Build identity: $ActualHash"
Write-Host "Build manifest ID: $($BuildManifest.manifestId)"
Write-Host "Run ID: $RunId"
if (-not $IsCameraIntegration -and $ChromeEngineHash -ne $ExpectedChromeEngineHash) {
    Write-Warning "ChromeEngine3.dll is missing or differs from the inspected baseline; its identity was recorded as '$ChromeEngineHash'."
}
Write-Host "Next: launch Call of Juarez manually, load a save, confirm normal rendering, then exit normally."
if ($IsReadbackDiagnostic) {
    Write-Host "After exit run tools\verify_d3d9_readback_live_test.ps1 -GameDirectory '$GameDirectory'."
} elseif ($IsOpenVrFlatDiagnostic) {
    Write-Host "After exit run tools\verify_d3d9_openvr_flat_live_test.ps1 -GameDirectory '$GameDirectory'."
} elseif ($IsCameraProbe) {
    Write-Host "Control file: '$CameraControl'."
    Write-Host "While gameplay is visible, run tools\set_camera_probe_control.ps1 to apply FOV/yaw/pitch."
    Write-Host "After exit run tools\verify_camera_probe_live_test.ps1 -GameDirectory '$GameDirectory'."
} elseif ($IsHmdCamera) {
    Write-Host "Start SteamVR manually before launching the game so the OpenVR pose source can initialize."
    Write-Host "Control file: '$CameraControl'."
    Write-Host "While gameplay is visible, use tools\set_hmd_camera_control.ps1 to enable/recenter/disable tracking."
    Write-Host "After exit run tools\verify_hmd_camera_live_test.ps1 -GameDirectory '$GameDirectory'."
} elseif ($IsNativeStereo) {
    Write-Host "Start SteamVR manually before launching the game so the native-stereo runtime can initialize."
    Write-Host "Control file: '$CameraControl'."
    Write-Host "In-headset recenter: press Create on the left PS VR2 Sense controller."
    Write-Host "Terminal recenter remains available through tools\set_hmd_camera_control.ps1 as a diagnostic fallback."
    Write-Host "After exit run tools\verify_native_stereo_live_test.ps1 -GameDirectory '$GameDirectory'."
} else {
    Write-Host "After exit run tools\verify_d3d9_live_test.ps1 -GameDirectory '$GameDirectory'."
}
