param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,

    [string]$ProxyPath = "",
    [string]$OpenVrDllPath = "",
    [string]$BuildManifestPath = "",
    [string]$RunId = "",
    [switch]$RequireSteamOverlayAbsent
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ProxyPath)) {
    $ProxyPath = Join-Path $PSScriptRoot "..\build\win32-debug\Release\d3d9_openvr_flat.dll"
}
if ([string]::IsNullOrWhiteSpace($OpenVrDllPath)) {
    $OpenVrDllPath = Join-Path $PSScriptRoot "..\build\win32-debug\Release\openvr_api.dll"
}

$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$ProxyPath = [System.IO.Path]::GetFullPath($ProxyPath)
$OpenVrDllPath = [System.IO.Path]::GetFullPath($OpenVrDllPath)
if ([string]::IsNullOrWhiteSpace($BuildManifestPath)) {
    $BuildManifestPath = Join-Path `
        ([System.IO.Path]::GetDirectoryName($ProxyPath)) `
        "$([System.IO.Path]::GetFileNameWithoutExtension($ProxyPath)).build-manifest.json"
}
$BuildManifestPath = [System.IO.Path]::GetFullPath($BuildManifestPath)
$OpenVrDestination = Join-Path $GameDirectory "openvr_api.dll"
$OpenVrBackup = Join-Path $GameDirectory "openvr_api.cojvr-backup.dll"
$State = Join-Path $GameDirectory ".cojvr-openvr-flat-stage.json"
$D3D9State = Join-Path $GameDirectory ".cojvr-d3d9-stage.json"

if (Get-Process -Name CoJ -ErrorAction SilentlyContinue) {
    throw "Call of Juarez is running. Close it before staging the OpenVR flat bridge."
}
if (-not (Test-Path -LiteralPath $ProxyPath -PathType Leaf)) {
    throw "OpenVR flat proxy was not found at '$ProxyPath'. Build Release first."
}
if (-not (Test-Path -LiteralPath $OpenVrDllPath -PathType Leaf)) {
    throw "openvr_api.dll was not found at '$OpenVrDllPath'. Build Release first."
}
if (-not (Test-Path -LiteralPath $BuildManifestPath -PathType Leaf)) {
    throw "Build manifest was not found at '$BuildManifestPath'. Create it before staging."
}
$BuildManifest = Get-Content -LiteralPath $BuildManifestPath -Raw | ConvertFrom-Json
if ([string]$BuildManifest.build.diagnosticMode -ne "d3d9_openvr_flat") {
    throw "Build manifest does not describe the OpenVR flat diagnostic."
}
$OpenVrArtifact = @($BuildManifest.artifacts | Where-Object { [string]$_.role -eq "openvr_runtime" })
if ($OpenVrArtifact.Count -ne 1) {
    throw "Build manifest must contain exactly one OpenVR runtime artifact."
}
$OpenVrSourceHash = (Get-FileHash -LiteralPath $OpenVrDllPath -Algorithm SHA256).Hash.ToUpperInvariant()
if ($OpenVrSourceHash -ne ([string]$OpenVrArtifact[0].sha256).ToUpperInvariant()) {
    throw "openvr_api.dll SHA-256 does not match the selected build manifest."
}
if (Test-Path -LiteralPath $State) {
    throw "OpenVR flat staging state already exists at '$State'. Unstage it first."
}
if (Test-Path -LiteralPath $OpenVrBackup) {
    throw "OpenVR backup '$OpenVrBackup' already exists. Restore it before staging."
}
if (Test-Path -LiteralPath $D3D9State) {
    throw "A D3D9 proxy is already staged. Unstage it before staging the OpenVR flat bridge."
}
if ([string]::IsNullOrWhiteSpace($RunId)) {
    $RunId = "{0}-{1}" -f [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssZ"), ([Guid]::NewGuid().ToString("N").Substring(0, 12))
}
if ($RunId -notmatch "^[A-Za-z0-9._-]+$") {
    throw "RunId may contain only letters, digits, '.', '_' and '-'."
}

$HadOriginalOpenVr = Test-Path -LiteralPath $OpenVrDestination
if ($HadOriginalOpenVr) {
    Move-Item -LiteralPath $OpenVrDestination -Destination $OpenVrBackup
}

try {
    Copy-Item -LiteralPath $OpenVrDllPath -Destination $OpenVrDestination

    & (Join-Path $PSScriptRoot "stage_d3d9_proxy.ps1") `
        -GameDirectory $GameDirectory `
        -ProxyPath $ProxyPath `
        -BuildManifestPath $BuildManifestPath `
        -RunId $RunId

    $CurrentRunPath = Join-Path $GameDirectory ".cojvr-run.json"
    $RunManifest = Get-Content -LiteralPath $CurrentRunPath -Raw | ConvertFrom-Json
    $RunManifest.deployment = @($RunManifest.deployment) + @(
        [ordered]@{
            role = "openvr_runtime"
            destination = "openvr_api.dll"
            sha256 = (Get-FileHash -LiteralPath $OpenVrDestination -Algorithm SHA256).Hash.ToUpperInvariant()
        }
    )
    $RunManifest | Add-Member -NotePropertyName validation -NotePropertyValue ([ordered]@{
        requireSteamOverlayAbsent = [bool]$RequireSteamOverlayAbsent
    }) -Force
    [System.IO.File]::WriteAllText(
        $CurrentRunPath,
        ($RunManifest | ConvertTo-Json -Depth 10) + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))
    $RunDirectory = Join-Path (Join-Path (Join-Path $GameDirectory ".cojvr-evidence") "runs") $RunId
    Copy-Item -LiteralPath $CurrentRunPath -Destination (Join-Path $RunDirectory "run-manifest.json") -Force

    @{
        schemaVersion = 1
        runId = $RunId
        buildManifestId = [string]$BuildManifest.manifestId
        hadOriginalOpenVr = $HadOriginalOpenVr
        stagedOpenVrSha256 = (Get-FileHash -LiteralPath $OpenVrDestination -Algorithm SHA256).Hash.ToUpperInvariant()
        requireSteamOverlayAbsent = [bool]$RequireSteamOverlayAbsent
    } | ConvertTo-Json | Set-Content -LiteralPath $State -Encoding UTF8
} catch {
    if (Test-Path -LiteralPath $D3D9State) {
        try {
            & (Join-Path $PSScriptRoot "unstage_d3d9_proxy.ps1") -GameDirectory $GameDirectory
        } catch {}
    }
    if (Test-Path -LiteralPath $OpenVrDestination) {
        Remove-Item -LiteralPath $OpenVrDestination -Force
    }
    if (Test-Path -LiteralPath $OpenVrBackup) {
        Move-Item -LiteralPath $OpenVrBackup -Destination $OpenVrDestination
    }
    throw
}

Write-Host "Staged sustained classic-D3D9 -> D3D11 -> OpenVR flat bridge."
Write-Host "SteamVR must already be running. Launch Call of Juarez manually in DX9."
Write-Host "Use menus, load a save and test gameplay for as long as needed, then close the game manually."
Write-Host "This diagnostic submits the same flat game frame to both eyes; stereo camera rendering is not enabled yet."
if ($RequireSteamOverlayAbsent) {
    Write-Host "This run requires Steam Overlay to be disabled; verification will reject factory CreateDevice ownership by gameoverlayrenderer.dll."
}
Write-Host "After exit run tools\verify_d3d9_openvr_flat_live_test.ps1 -GameDirectory '$GameDirectory'."
