param(
    [ValidateSet("prepare", "enable", "recenter", "disable", "status", "finish")]
    [string]$Action = "prepare",

    [string]$GameDirectory = ""
)

$ErrorActionPreference = "Stop"

$RepositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$LocalStateDirectory = Join-Path $RepositoryRoot "work"
$LocalConfigPath = Join-Path $LocalStateDirectory "vr-test.json"

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

        & (Join-Path $PSScriptRoot "stage_d3d9_proxy.ps1") `
            -GameDirectory $ResolvedGameDirectory `
            -ProxyPath $ProxyPath `
            -BuildManifestPath $ManifestPath
        if ($LASTEXITCODE -ne 0) { throw "Native-stereo staging failed." }

        & (Join-Path $PSScriptRoot "set_hmd_camera_control.ps1") `
            -GameDirectory $ResolvedGameDirectory `
            -Mode enable

        $Run = Get-Content -LiteralPath $CurrentRunPath -Raw | ConvertFrom-Json
        Write-Host ""
        Write-Host "Native-stereo VR test candidate ready."
        Write-Host "Run ID: $($Run.runId)"
        Write-Host "Tracking/stereo: enabled; first valid HMD pose becomes the base orientation."
        Write-Host "Start SteamVR manually, then Call of Juarez."
        Write-Host "Recenter in-headset: press Create on the left PS VR2 Sense controller."
        Write-Host "Diagnostic fallback: pwsh -File tools\vr_test.ps1 recenter"
        Write-Host "Before closing the game: pwsh -File tools\vr_test.ps1 disable"
        Write-Host "After closing the game: pwsh -File tools\vr_test.ps1 finish"
        break
    }

    { $_ -in @("enable", "recenter", "disable") } {
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
            Write-Host "Recenter requested: $([bool]$Control.recenter)"
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

        $CollectionError = $null
        try {
            & (Join-Path $PSScriptRoot "collect_run_evidence.ps1") `
                -GameDirectory $ResolvedGameDirectory
        } catch {
            $CollectionError = $_.Exception.Message
            Write-Warning "Evidence collection failed: $CollectionError"
        }

        & (Join-Path $PSScriptRoot "unstage_d3d9_proxy.ps1") `
            -GameDirectory $ResolvedGameDirectory

        if ($CollectionError) {
            throw "Candidate was unstaged, but evidence collection failed: $CollectionError"
        }
        if ($VerificationError) {
            throw "Candidate evidence was collected and the game directory was restored, but the live verifier failed: $VerificationError"
        }
        Write-Host "VR test finalized: evidence verified/collected and staging restored."
        break
    }
}
