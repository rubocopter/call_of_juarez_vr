param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,

    [string]$ProxyPath = "",
    [string]$BuildManifestPath = "",
    [string]$RunId = ""
)

$ErrorActionPreference = "Stop"

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
$OpenVrDestination = Join-Path $GameDirectory "openvr_api.dll"
$OpenVrBackup = Join-Path $GameDirectory "openvr_api.cojvr-backup.dll"

$ProxyLeaf = [System.IO.Path]::GetFileName($ProxyPath)
$IsReadbackDiagnostic = $ProxyLeaf -ieq "d3d9_readback.dll"
$IsOpenVrFlatDiagnostic = $ProxyLeaf -ieq "d3d9_openvr_flat.dll"
$IsCameraProbe = $ProxyLeaf -ieq "d3d9_camera_probe.dll"
$IsHmdCamera = $ProxyLeaf -ieq "d3d9_hmd_camera.dll"
$IsCameraIntegration = $IsCameraProbe -or $IsHmdCamera
$DiagnosticMode = if ($IsReadbackDiagnostic) {
    "d3d9_readback"
} elseif ($IsOpenVrFlatDiagnostic) {
    "d3d9_openvr_flat"
} elseif ($IsCameraProbe) {
    "d3d9_camera_probe"
} elseif ($IsHmdCamera) {
    "d3d9_hmd_camera"
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
if ($IsHmdCamera) {
    if ($OpenVrArtifact.Count -ne 1) {
        throw "HMD camera manifest must contain exactly one OpenVR runtime artifact."
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

$ActualHash = (Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash.ToUpperInvariant()
if ($ActualHash -ne $ExpectedCoJHash) {
    throw "CoJ.exe SHA-256 is not the known exact build. Expected $ExpectedCoJHash, got $ActualHash."
}

$ChromeEngineHash = if (Test-Path -LiteralPath $ChromeEngine -PathType Leaf) {
    (Get-FileHash -LiteralPath $ChromeEngine -Algorithm SHA256).Hash.ToUpperInvariant()
} else {
    $null
}

if ($IsCameraIntegration -and $ChromeEngineHash -ne $ExpectedChromeEngineHash) {
    throw "Camera probe requires the exact inspected ChromeEngine3.dll. Expected $ExpectedChromeEngineHash, got '$ChromeEngineHash'."
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
if ($IsHmdCamera -and (Test-Path -LiteralPath $OpenVrBackup)) {
    throw "OpenVR backup '$OpenVrBackup' already exists. Restore or remove it before staging the HMD camera candidate."
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
$HistoricalDirectory = Join-Path `
    (Join-Path $EvidenceRoot "historical") `
    ("{0}-{1}" -f [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssZ"), ([Guid]::NewGuid().ToString("N").Substring(0, 8)))
New-Item -ItemType Directory -Path $RunDirectory -Force | Out-Null
Copy-Item -LiteralPath $BuildManifestPath -Destination (Join-Path $RunDirectory "build-manifest.json")

$HistoricalFiles = @($Log, (Join-Path $GameDirectory "callstack.txt"), $CurrentRun)
foreach ($HistoricalFile in $HistoricalFiles) {
    if (Test-Path -LiteralPath $HistoricalFile -PathType Leaf) {
        New-Item -ItemType Directory -Path $HistoricalDirectory -Force | Out-Null
        Move-Item -LiteralPath $HistoricalFile -Destination `
            (Join-Path $HistoricalDirectory ([System.IO.Path]::GetFileName($HistoricalFile)))
    }
}

$HadOriginal = Test-Path -LiteralPath $Destination
$HadOriginalCameraControl = $IsCameraIntegration -and (Test-Path -LiteralPath $CameraControl -PathType Leaf)
$HadOriginalOpenVr = $IsHmdCamera -and (Test-Path -LiteralPath $OpenVrDestination -PathType Leaf)
if ($HadOriginal) {
    Move-Item -LiteralPath $Destination -Destination $Backup
    Write-Host "Backed up existing d3d9.dll to d3d9.cojvr-backup.dll"
}

try {
    Copy-Item -LiteralPath $ProxyPath -Destination $Destination
    if ($IsHmdCamera) {
        if ($HadOriginalOpenVr) {
            Move-Item -LiteralPath $OpenVrDestination -Destination $OpenVrBackup
        }
        Copy-Item -LiteralPath $OpenVrSource -Destination $OpenVrDestination
    }
    if ($IsCameraIntegration) {
        if ($HadOriginalCameraControl) {
            Move-Item -LiteralPath $CameraControl -Destination $CameraControlBackup
        }
        [System.IO.File]::WriteAllText(
            $CameraControl,
            "{`n  `"enabled`": false,`n  `"trackingEnabled`": false,`n  `"recenter`": false,`n  `"yawDegrees`": 0,`n  `"pitchDegrees`": 0`n}`n",
            [System.Text.UTF8Encoding]::new($false))
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
        openVrRuntimeManaged = $IsHmdCamera
        hadOriginalOpenVr = $HadOriginalOpenVr
        stagedOpenVrSha256 = if ($IsHmdCamera) {
            (Get-FileHash -LiteralPath $OpenVrDestination -Algorithm SHA256).Hash.ToUpperInvariant()
        } else { $null }
    } | ConvertTo-Json | Set-Content -LiteralPath $State -Encoding UTF8

    $Deployment = @(
        [ordered]@{
            role = "proxy"
            destination = "d3d9.dll"
            sha256 = (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash.ToUpperInvariant()
        }
    )
    if ($IsHmdCamera) {
        $Deployment += [ordered]@{
            role = "openvr_runtime"
            destination = "openvr_api.dll"
            sha256 = (Get-FileHash -LiteralPath $OpenVrDestination -Algorithm SHA256).Hash.ToUpperInvariant()
        }
    }

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
            requireExactChromeEngine = $IsCameraIntegration
        }
        deployment = @($Deployment)
    }
    $RunJson = $RunManifest | ConvertTo-Json -Depth 10
    [System.IO.File]::WriteAllText(
        $CurrentRun,
        $RunJson + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))
    Copy-Item -LiteralPath $CurrentRun -Destination (Join-Path $RunDirectory "run-manifest.json") -Force
} catch {
    if ($IsCameraIntegration) {
        if (Test-Path -LiteralPath $CameraControl -PathType Leaf) {
            Remove-Item -LiteralPath $CameraControl -Force
        }
        if ($HadOriginalCameraControl -and (Test-Path -LiteralPath $CameraControlBackup -PathType Leaf)) {
            Move-Item -LiteralPath $CameraControlBackup -Destination $CameraControl
        }
    }
    if ($IsHmdCamera) {
        if (Test-Path -LiteralPath $OpenVrDestination -PathType Leaf) {
            $CurrentOpenVrHash = (Get-FileHash -LiteralPath $OpenVrDestination -Algorithm SHA256).Hash.ToUpperInvariant()
            if ($CurrentOpenVrHash -eq $OpenVrHash) {
                Remove-Item -LiteralPath $OpenVrDestination -Force
            }
        }
        if ($HadOriginalOpenVr -and (Test-Path -LiteralPath $OpenVrBackup -PathType Leaf)) {
            Move-Item -LiteralPath $OpenVrBackup -Destination $OpenVrDestination
        }
    }
    if (Test-Path -LiteralPath $Destination) {
        Remove-Item -LiteralPath $Destination -Force
    }
    if (Test-Path -LiteralPath $Backup) {
        Move-Item -LiteralPath $Backup -Destination $Destination
    }
    if (Test-Path -LiteralPath $State) {
        Remove-Item -LiteralPath $State -Force
    }
    if (Test-Path -LiteralPath $CurrentRun) {
        Remove-Item -LiteralPath $CurrentRun -Force
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
} else {
    Write-Host "After exit run tools\verify_d3d9_live_test.ps1 -GameDirectory '$GameDirectory'."
}
