param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,

    [Parameter(Mandatory = $true)]
    [string]$ProxyPath,

    [Parameter(Mandatory = $true)]
    [string]$BuildManifestPath
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
        proxyHash = Get-CojvrFileSha256 (Join-Path $Fixture "d3d9.dll")
        openVrHash = Get-CojvrFileSha256 (Join-Path $Fixture "openvr_api.dll")
        cameraHash = Get-CojvrFileSha256 (Join-Path $Fixture "cojvr-camera-control.json")
        inputManifest = @(Get-CojvrDirectoryManifest (Join-Path $Fixture "cojvr_openvr_input"))
    }
}

function Assert-FixtureRecovered([object]$Fixture) {
    $Path = [string]$Fixture.path
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
        -ValidationProfile full | Out-Null
}

$StageCheckpoints = @(
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
    for ($Cycle = 1; $Cycle -le 2; ++$Cycle) {
        Invoke-TestStage ([string]$CycleFixture.path)
        Assert-True (Test-Path -LiteralPath (Join-Path ([string]$CycleFixture.path) ".cojvr-d3d9-stage.json")) `
            "Stage/unstage cycle $Cycle did not create staging state."
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
