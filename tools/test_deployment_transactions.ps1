param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,

    [Parameter(Mandatory = $true)]
    [string]$ProxyPath,

    [Parameter(Mandatory = $true)]
    [string]$BuildManifestPath,
    [switch]$IndependentHands
)

$ErrorActionPreference = "Stop"

$RepositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$ProxyPath = [System.IO.Path]::GetFullPath($ProxyPath)
$BuildManifestPath = [System.IO.Path]::GetFullPath($BuildManifestPath)
. (Join-Path $PSScriptRoot "deployment_transaction.ps1")

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$TempRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd("\") + "\"
$TestRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $TempRoot ("cojvr-deployment-integration-" + [Guid]::NewGuid().ToString("N"))))
if (-not $TestRoot.StartsWith($TempRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to create the deployment integration fixture outside the system temp directory."
}

function New-DeploymentFixture([string]$Name) {
    $Fixture = Join-Path $TestRoot $Name
    New-Item -ItemType Directory -Path (Join-Path $Fixture "cojvr_openvr_input\bindings") -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $GameDirectory "CoJ.exe") -Destination (Join-Path $Fixture "CoJ.exe")
    Copy-Item -LiteralPath (Join-Path $GameDirectory "ChromeEngine3.dll") -Destination (Join-Path $Fixture "ChromeEngine3.dll")
    $CodeSource = Join-Path $GameDirectory "code.cojvr-backup.pak"
    if (-not (Test-Path -LiteralPath $CodeSource)) { $CodeSource = Join-Path $GameDirectory "code.pak" }
    Copy-Item -LiteralPath $CodeSource -Destination (Join-Path $Fixture "code.pak")
    $PlayerHash=$null
    if ($IndependentHands) {
        $PlayerSource=Join-Path $GameDirectory "Data0.cojvr-backup.pak"
        if (-not (Test-Path -LiteralPath $PlayerSource)) { $PlayerSource=Join-Path $GameDirectory "Data0.pak" }
        Copy-Item -LiteralPath $PlayerSource -Destination (Join-Path $Fixture "Data0.pak")
        $PlayerHash=Get-CojvrFileSha256 (Join-Path $Fixture "Data0.pak")
    }
    [System.IO.File]::WriteAllBytes((Join-Path $Fixture "d3d9.dll"), [byte[]](1, 2, 3, 4, 5))
    [System.IO.File]::WriteAllBytes((Join-Path $Fixture "openvr_api.dll"), [byte[]](6, 7, 8, 9))
    [System.IO.File]::WriteAllText(
        (Join-Path $Fixture "cojvr-camera-control.json"),
        '{"original":true}',
        [System.Text.UTF8Encoding]::new($false))
    [System.IO.File]::WriteAllText(
        (Join-Path $Fixture "cojvr_openvr_input\original.json"),
        '{"original":true}',
        [System.Text.UTF8Encoding]::new($false))
    return [pscustomobject]@{
        path = $Fixture
        codeHash = Get-CojvrFileSha256 (Join-Path $Fixture "code.pak")
        playerHash = $PlayerHash
        proxyHash = Get-CojvrFileSha256 (Join-Path $Fixture "d3d9.dll")
        openVrHash = Get-CojvrFileSha256 (Join-Path $Fixture "openvr_api.dll")
        cameraHash = Get-CojvrFileSha256 (Join-Path $Fixture "cojvr-camera-control.json")
        inputManifest = @(Get-CojvrDirectoryManifest (Join-Path $Fixture "cojvr_openvr_input"))
    }
}

function Assert-FixtureRecovered([object]$Fixture) {
    $Path = [string]$Fixture.path
    if ($IndependentHands) {
        Assert-True ((Get-CojvrFileSha256 (Join-Path $Path "Data0.pak")) -eq [string]$Fixture.playerHash) `
            "Original Data0.pak was not restored byte-for-byte."
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $Path "Data0.cojvr-backup.pak"))) `
            "Recovered fixture retained the player geometry backup."
    }
    Assert-True ((Get-CojvrFileSha256 (Join-Path $Path "code.pak")) -eq [string]$Fixture.codeHash) `
        "Original code.pak was not restored byte-for-byte."
    Assert-True ((Get-CojvrFileSha256 (Join-Path $Path "d3d9.dll")) -eq [string]$Fixture.proxyHash) `
        "Fixture proxy was not restored byte-for-byte."
    Assert-True ((Get-CojvrFileSha256 (Join-Path $Path "openvr_api.dll")) -eq [string]$Fixture.openVrHash) `
        "Fixture OpenVR runtime was not restored byte-for-byte."
    Assert-True ((Get-CojvrFileSha256 (Join-Path $Path "cojvr-camera-control.json")) -eq [string]$Fixture.cameraHash) `
        "Fixture camera-control file was not restored byte-for-byte."
    Assert-True (Test-CojvrDirectoryMatchesManifest `
        (Join-Path $Path "cojvr_openvr_input") @($Fixture.inputManifest)) `
        "Fixture OpenVR input directory was not restored exactly."
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $Path ".cojvr-d3d9-stage.json"))) `
        "Recovered fixture retained staging state."
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $Path ".cojvr-deployment-transaction.json"))) `
        "Recovered fixture retained a transaction journal."
}

function Invoke-TestStage([string]$FixturePath) {
    & (Join-Path $PSScriptRoot "stage_d3d9_proxy.ps1") `
        -GameDirectory $FixturePath `
        -ProxyPath $ProxyPath `
        -BuildManifestPath $BuildManifestPath `
        -ValidationProfile full -IndependentHands:$IndependentHands | Out-Null
}

$StageCheckpoints = @(
    "stage_code_backup_created",
    "stage_code_published",
    "journal_written",
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
$UnstageCheckpoints = @(
    "unstage_code_removed",
    "unstage_code_original_restored",
    "journal_written",
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

if ($IndependentHands) {
    $StageCheckpoints=@("stage_player_backup_created", "stage_player_published")
    $UnstageCheckpoints=@("unstage_player_removed", "unstage_player_original_restored")
}

$PreviousFailureTest = $env:COJVR_DEPLOYMENT_FAILURE_TEST
$PreviousFailureCheckpoint = $env:COJVR_DEPLOYMENT_FAIL_AFTER
try {
    New-Item -ItemType Directory -Path $TestRoot -Force | Out-Null

    foreach ($Checkpoint in $StageCheckpoints) {
        $Fixture = New-DeploymentFixture ("stage-" + $Checkpoint)
        $env:COJVR_DEPLOYMENT_FAILURE_TEST = "1"
        $env:COJVR_DEPLOYMENT_FAIL_AFTER = $Checkpoint
        $Rejected = $false
        try {
            Invoke-TestStage ([string]$Fixture.path)
        } catch {
            $Rejected = $_.Exception.Message -match "Injected deployment failure"
        }
        Assert-True $Rejected "Stage checkpoint '$Checkpoint' was not reached."
        $env:COJVR_DEPLOYMENT_FAILURE_TEST = $null
        $env:COJVR_DEPLOYMENT_FAIL_AFTER = $null
        if (Test-Path -LiteralPath (Join-Path ([string]$Fixture.path) ".cojvr-deployment-transaction.json")) {
            [void](Complete-CojvrDeploymentRecovery ([string]$Fixture.path))
        }
        Assert-FixtureRecovered $Fixture
    }

    $CycleFixture = New-DeploymentFixture "repeated-cycle"
    if ($IndependentHands) {
        $Player=Join-Path ([string]$CycleFixture.path) "Data0.pak"
        $Size=(Get-Item -LiteralPath $Player).Length
        $Stream=[System.IO.File]::Open($Player,[System.IO.FileMode]::Append)
        try { $Stream.WriteByte(0) } finally { $Stream.Dispose() }
        $Rejected=$false
        try { Invoke-TestStage ([string]$CycleFixture.path) } catch {
            $Rejected=$_.Exception.Message -match "Unknown Data0.pak SHA-256"
        }
        Assert-True $Rejected "Unknown player geometry archive was accepted."
        $Stream=[System.IO.File]::Open($Player,[System.IO.FileMode]::Open)
        try { $Stream.SetLength($Size) } finally { $Stream.Dispose() }
        Assert-FixtureRecovered $CycleFixture
    }
    for ($Cycle = 1; $Cycle -le 2; ++$Cycle) {
        Invoke-TestStage ([string]$CycleFixture.path)
        Assert-True (Test-Path -LiteralPath (Join-Path ([string]$CycleFixture.path) ".cojvr-d3d9-stage.json")) `
            "Stage/unstage cycle $Cycle did not create staging state."
        if ($IndependentHands) {
            $Run=Get-Content -LiteralPath (Join-Path ([string]$CycleFixture.path) ".cojvr-run.json") -Raw | ConvertFrom-Json
            Assert-True ($Run.validation.handPresentation -eq "independent_native_hands" -and
                @($Run.deployment | Where-Object { $_.role -eq "game_player_geometry" }).Count -eq 1) `
                "Player geometry was not correlated with the hand presentation in run provenance."
            if ($Cycle -eq 1) {
                $Player=Join-Path ([string]$CycleFixture.path) "Data0.pak"
                $Size=(Get-Item -LiteralPath $Player).Length
                $Stream=[System.IO.File]::Open($Player,[System.IO.FileMode]::Append)
                try { $Stream.WriteByte(0) } finally { $Stream.Dispose() }
                $Rejected=$false
                try { & (Join-Path $PSScriptRoot "unstage_d3d9_proxy.ps1") -GameDirectory ([string]$CycleFixture.path) | Out-Null } catch {
                    $Rejected=$_.Exception.Message -match "Data0.pak or its original backup changed"
                }
                Assert-True $Rejected "Changed deployed player geometry was overwritten during unstage."
                Assert-True ((Get-CojvrFileSha256 (Join-Path ([string]$CycleFixture.path) "Data0.cojvr-backup.pak")) -eq [string]$CycleFixture.playerHash) `
                    "Rejected player restoration changed its original backup."
                $Stream=[System.IO.File]::Open($Player,[System.IO.FileMode]::Open)
                try { $Stream.SetLength($Size) } finally { $Stream.Dispose() }
            }
        }
        & (Join-Path $PSScriptRoot "unstage_d3d9_proxy.ps1") -GameDirectory ([string]$CycleFixture.path) | Out-Null
        Assert-FixtureRecovered $CycleFixture
    }

    foreach ($Checkpoint in $UnstageCheckpoints) {
        $Fixture = New-DeploymentFixture ("unstage-" + $Checkpoint)
        Invoke-TestStage ([string]$Fixture.path)
        $env:COJVR_DEPLOYMENT_FAILURE_TEST = "1"
        $env:COJVR_DEPLOYMENT_FAIL_AFTER = $Checkpoint
        $Rejected = $false
        try {
            & (Join-Path $PSScriptRoot "unstage_d3d9_proxy.ps1") -GameDirectory ([string]$Fixture.path) | Out-Null
        } catch {
            $Rejected = $_.Exception.Message -match "Injected deployment failure"
        }
        Assert-True $Rejected "Unstage checkpoint '$Checkpoint' was not reached."
        $env:COJVR_DEPLOYMENT_FAILURE_TEST = $null
        $env:COJVR_DEPLOYMENT_FAIL_AFTER = $null
        if (Test-Path -LiteralPath (Join-Path ([string]$Fixture.path) ".cojvr-deployment-transaction.json")) {
            [void](Complete-CojvrDeploymentRecovery ([string]$Fixture.path))
        }
        Assert-FixtureRecovered $Fixture
    }

    Write-Host "PASS - deployment transaction integration: $($StageCheckpoints.Count) stage failures, $($UnstageCheckpoints.Count) unstage failures, 2 repeated cycles."
} finally {
    $env:COJVR_DEPLOYMENT_FAILURE_TEST = $PreviousFailureTest
    $env:COJVR_DEPLOYMENT_FAIL_AFTER = $PreviousFailureCheckpoint
    if ((Test-Path -LiteralPath $TestRoot -PathType Container) -and
        $TestRoot.StartsWith($TempRoot, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $TestRoot -Recurse -Force
    }
}
