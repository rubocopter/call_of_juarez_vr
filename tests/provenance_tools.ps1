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
        "stage_d3d9_proxy.ps1",
        "stage_d3d9_openvr_flat.ps1",
        "set_camera_probe_control.ps1",
        "verify_camera_probe_live_test.ps1",
        "set_hmd_camera_control.ps1",
        "verify_hmd_camera_live_test.ps1",
        "verify_native_stereo_live_test.ps1",
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

    $StereoOrientation1 = "camera_probe_event: event=camera_hmd_orientation_applied result=ok pose_sequence=2 yaw_degrees=10 pitch_degrees=0 natural_determinant=1 applied_determinant=1 native_homogeneous_layout=true source_world_homogeneous_layout=true source_view_homogeneous_layout=true injected_view_homogeneous_layout=true render_basis_observed=true render_basis_changed=true view_matrix_observed=true view_matrix_changed=true projection_matrix_changed=true view_projection_changed=true restore_deferred=true;stereo=true;renderer_camera_match=true"
    $StereoOrientation2 = "camera_probe_event: event=camera_hmd_orientation_applied result=ok pose_sequence=3 yaw_degrees=0 pitch_degrees=5 natural_determinant=1 applied_determinant=1 native_homogeneous_layout=true source_world_homogeneous_layout=true source_view_homogeneous_layout=true injected_view_homogeneous_layout=true render_basis_observed=true render_basis_changed=true view_matrix_observed=true view_matrix_changed=true projection_matrix_changed=true view_projection_changed=true restore_deferred=true;stereo=true;renderer_camera_match=true"
    $StereoFrame1 = "camera_probe_event: event=camera_native_stereo_frame result=ok frame_sequence=1 pose_sequence=2 left_camera_applied=true left_projection_applied=true left_captured=true left_state_restored=true right_rendered=true right_full_view_pass=true right_view_guard_restored=true right_captured=true right_state_restored=true submitted=true left_renderer_camera_match=true right_renderer_camera_match=true left_hash=111 right_hash=222 distinct_eye_content=true left_eye_x=-0.032 right_eye_x=0.032 left_eye_position=(-0.032000,0.000000,0.000000) right_eye_position=(0.032000,0.000000,0.000000) game_units_per_meter=100 left_applied_position=(96.8000,200.0000,300.0000) right_applied_position=(103.2000,200.0000,300.0000) left_fov=-1,0.8,-1,1 right_fov=-0.8,1,-1,1 left_frustum=-2,1.5,-1.8,1.8,1,1000 right_frustum=-1.5,2,-1.8,1.8,1,1000 render_view_rva=0x30fb0 render_core_rva=0x30e00"
    $StereoFrame2 = "camera_probe_event: event=camera_native_stereo_frame result=ok frame_sequence=2 pose_sequence=3 left_camera_applied=true left_projection_applied=true left_captured=true left_state_restored=true right_rendered=true right_full_view_pass=true right_view_guard_restored=true right_captured=true right_state_restored=true submitted=true left_renderer_camera_match=true right_renderer_camera_match=true left_hash=333 right_hash=444 distinct_eye_content=true left_eye_x=-0.032 right_eye_x=0.032 left_eye_position=(-0.032000,0.000000,0.000000) right_eye_position=(0.032000,0.000000,0.000000) game_units_per_meter=100 left_applied_position=(46.8000,60.0000,70.0000) right_applied_position=(53.2000,60.0000,70.0000) left_fov=-1,0.8,-1,1 right_fov=-0.8,1,-1,1 left_frustum=-2,1.5,-1.8,1.8,1,1000 right_frustum=-1.5,2,-1.8,1.8,1,1000 render_view_rva=0x30fb0 render_core_rva=0x30e00"
    $StereoVerifierLog = @(
        "run_start: run_id=$StereoVerifierRunId build_manifest_id=$($StereoBuildManifest.manifestId) pid=789",
        "native_stereo_runtime: status=started backend=openvr recommended_eye=2000x2040 left_eye_x=-0.032 right_eye_x=0.032 pose_semantics=eye_to_head",
        "native_stereo_factory_hook: status=installed",
        "native_stereo_device: status=observed device=0x1234",
        "camera_probe_bootstrap: status=installed system_d3d9=expected",
        "camera_probe_event: event=camera_probe_install result=installed pose_source=none native_stereo=available",
        "camera_probe_event: event=camera_probe_control_loaded result=accepted generation=1 tracking_enabled=true",
        "camera_probe_event: event=camera_hmd_recentered result=ok generation=1 pose_sequence=1 stereo=true",
        "native_stereo_capture: status=source eye=left transport=classic_d3d9_cpu_readback;capture_source=render_target0;source_is_backbuffer=true;eye_surface=2560x1440;viewport=0,0,2560,1440,0,1;format=21;msaa=0;feature_level=0xb000",
        "native_stereo_capture: status=source eye=right transport=classic_d3d9_cpu_readback;capture_source=render_target0;source_is_backbuffer=true;eye_surface=2560x1440;viewport=0,0,2560,1440,0,1;format=21;msaa=0;feature_level=0xb000",
        $StereoOrientation1,
        $StereoFrame1,
        $StereoOrientation2,
        $StereoFrame2,
        "native_stereo_capture_timing: status=ok frame_sequence=2 eye=left transport=classic_d3d9_cpu_readback;capture_source=render_target0;source_is_backbuffer=true;eye_surface=2560x1440;viewport=0,0,2560,1440,0,1;format=21;msaa=0;feature_level=0xb000;gpu_readback_ms=4.000;copy_upload_ms=1.000;capture_total_ms=5.000",
        "native_stereo_capture_timing: status=ok frame_sequence=2 eye=right transport=classic_d3d9_cpu_readback;capture_source=render_target0;source_is_backbuffer=true;eye_surface=2560x1440;viewport=0,0,2560,1440,0,1;format=21;msaa=0;feature_level=0xb000;gpu_readback_ms=4.100;copy_upload_ms=1.100;capture_total_ms=5.200",
        "native_stereo_submit_timing: status=ok frame_sequence=2 transport=classic_d3d9_cpu_readback;capture_source=render_target0;source_is_backbuffer=true;eye_surface=2560x1440;viewport=0,0,2560,1440,0,1;format=21;msaa=0;feature_level=0xb000;gpu_readback_ms=4.100;copy_upload_ms=1.100;capture_total_ms=5.200;submit_ms=0.400",
        "openvr_input: status=started action_set=/actions/global recenter=/actions/global/in/recenter binding=psvr2_sense_create",
        "openvr_input_event: action=recenter result=pressed source=global_action",
        "camera_probe_event: event=camera_hmd_recenter_requested result=ok detail=source=openvr_global_action;pose_sequence=3",
        "camera_probe_event: event=camera_probe_control_loaded result=accepted generation=2 tracking_enabled=false",
        "camera_probe_event: event=camera_probe_passthrough result=disabled",
        "camera_probe_event: event=camera_probe_restore result=restored camera_restored_slots=2;view_restored_slots=1",
        "native_stereo_factory_hook: status=restored",
        "native_stereo_runtime: status=stopped",
        "run_end: run_id=$StereoVerifierRunId"
    )
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierLog
    & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
        -GameDirectory $StereoVerifierGame | Out-Null

    $StereoVerifierFlat = @($StereoVerifierLog)
    $StereoVerifierFlat[11] = $StereoFrame1 -replace "distinct_eye_content=true", "distinct_eye_content=false" -replace "right_hash=222", "right_hash=111"
    $StereoVerifierFlat[13] = $StereoFrame2 -replace "distinct_eye_content=true", "distinct_eye_content=false" -replace "right_hash=444", "right_hash=333"
    Set-Content -LiteralPath (Join-Path $StereoVerifierGame "cojvr.log") -Encoding UTF8 -Value $StereoVerifierFlat
    $FlatStereoRejected = $false
    try {
        & (Join-Path $SourceDirectory "tools\verify_native_stereo_live_test.ps1") `
            -GameDirectory $StereoVerifierGame | Out-Null
    } catch {
        $FlatStereoRejected = $true
    }
    Assert-True $FlatStereoRejected `
        "Native-stereo verifier accepted identical left/right eye content."

    $StereoVerifierWrongCamera = @($StereoVerifierLog)
    $StereoVerifierWrongCamera[11] = $StereoFrame1 -replace "right_renderer_camera_match=true", "right_renderer_camera_match=false"
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

    $StereoVerifierCoreOnly = @($StereoVerifierLog)
    $StereoVerifierCoreOnly[11] = $StereoFrame1 -replace "right_full_view_pass=true", "right_full_view_pass=false"
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

    $StereoVerifierWrongScale = @($StereoVerifierLog)
    $StereoVerifierWrongScale[11] = $StereoFrame1 `
        -replace "game_units_per_meter=100", "game_units_per_meter=1" `
        -replace "left_applied_position=\(96.8000,200.0000,300.0000\)", "left_applied_position=(99.9680,200.0000,300.0000)" `
        -replace "right_applied_position=\(103.2000,200.0000,300.0000\)", "right_applied_position=(100.0320,200.0000,300.0000)"
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

    $StereoVerifierMissingViewport = @($StereoVerifierLog)
    $StereoVerifierMissingViewport[8] = $StereoVerifierMissingViewport[8] -replace "viewport=0,0,2560,1440,0,1", "viewport=unavailable"
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
