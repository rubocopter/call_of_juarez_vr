param(
    [string]$RepositoryRoot = "",

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("d3d9_forwarding", "d3d9_readback", "d3d9_openvr_flat", "d3d9_camera_probe", "d3d9_hmd_camera")]
    [string]$DiagnosticMode = "d3d9_forwarding",

    [string]$ProxyPath = "",
    [string]$OpenVrDllPath = "",
    [string]$OutputPath = "",
    [switch]$AllowDirty
)

$ErrorActionPreference = "Stop"

function Get-UpperSha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Get-TextSha256([string]$Text) {
    $Algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        $Bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
        return ([BitConverter]::ToString($Algorithm.ComputeHash($Bytes))).Replace("-", "")
    } finally {
        $Algorithm.Dispose()
    }
}

function Invoke-Git([string[]]$Arguments) {
    $Output = & git -C $RepositoryRoot @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed: $($Output -join [Environment]::NewLine)"
    }
    return @($Output)
}

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Join-Path $PSScriptRoot ".."
}
$RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)

$DefaultProxyNames = @{
    d3d9_forwarding = "d3d9.dll"
    d3d9_readback = "d3d9_readback.dll"
    d3d9_openvr_flat = "d3d9_openvr_flat.dll"
    d3d9_camera_probe = "d3d9_camera_probe.dll"
    d3d9_hmd_camera = "d3d9_hmd_camera.dll"
}
if ([string]::IsNullOrWhiteSpace($ProxyPath)) {
    $ProxyPath = Join-Path $RepositoryRoot "build\win32-debug\$Configuration\$($DefaultProxyNames[$DiagnosticMode])"
}
$ProxyPath = [System.IO.Path]::GetFullPath($ProxyPath)
if (-not (Test-Path -LiteralPath $ProxyPath -PathType Leaf)) {
    throw "Proxy artifact was not found at '$ProxyPath'. Build $Configuration first."
}

if ($DiagnosticMode -in @("d3d9_openvr_flat", "d3d9_hmd_camera")) {
    if ([string]::IsNullOrWhiteSpace($OpenVrDllPath)) {
        $OpenVrDllPath = Join-Path ([System.IO.Path]::GetDirectoryName($ProxyPath)) "openvr_api.dll"
    }
    $OpenVrDllPath = [System.IO.Path]::GetFullPath($OpenVrDllPath)
    if (-not (Test-Path -LiteralPath $OpenVrDllPath -PathType Leaf)) {
        throw "OpenVR runtime artifact was not found at '$OpenVrDllPath'. Build $Configuration first."
    }
}

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path `
        ([System.IO.Path]::GetDirectoryName($ProxyPath)) `
        "$([System.IO.Path]::GetFileNameWithoutExtension($ProxyPath)).build-manifest.json"
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

$HeadCommitOutput = @(Invoke-Git @("rev-parse", "HEAD"))
$HeadTreeOutput = @(Invoke-Git @("rev-parse", "HEAD^{tree}"))
$BranchOutput = @(Invoke-Git @("branch", "--show-current"))
$HeadCommit = ([string]$HeadCommitOutput[0]).Trim()
$HeadTree = ([string]$HeadTreeOutput[0]).Trim()
$Branch = if ($BranchOutput.Count -gt 0) { ([string]$BranchOutput[0]).Trim() } else { "" }
$StatusLines = @(Invoke-Git @("status", "--porcelain=v1", "--untracked-files=all")) |
    Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
$Dirty = $StatusLines.Count -ne 0
if ($Dirty -and -not $AllowDirty) {
    throw "The source tree is dirty. Commit/stash changes or pass -AllowDirty to create a non-commit-reproducible manifest."
}

$DiffText = (& git -c core.autocrlf=false -c core.safecrlf=false `
    -C $RepositoryRoot diff --binary HEAD 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) {
    throw "git diff --binary HEAD failed."
}
$Untracked = @($StatusLines | Where-Object { $_.StartsWith("?? ") } | ForEach-Object {
    $RelativePath = $_.Substring(3)
    $AbsolutePath = Join-Path $RepositoryRoot $RelativePath
    if (Test-Path -LiteralPath $AbsolutePath -PathType Leaf) {
        [ordered]@{
            path = $RelativePath.Replace("\", "/")
            sha256 = Get-UpperSha256 $AbsolutePath
            size = (Get-Item -LiteralPath $AbsolutePath).Length
        }
    }
})

$Artifacts = @(
    [ordered]@{
        role = "proxy"
        fileName = [System.IO.Path]::GetFileName($ProxyPath)
        sourcePath = $ProxyPath
        sha256 = Get-UpperSha256 $ProxyPath
        size = (Get-Item -LiteralPath $ProxyPath).Length
    }
)
if ($DiagnosticMode -in @("d3d9_openvr_flat", "d3d9_hmd_camera")) {
    $Artifacts += [ordered]@{
        role = "openvr_runtime"
        fileName = [System.IO.Path]::GetFileName($OpenVrDllPath)
        sourcePath = $OpenVrDllPath
        sha256 = Get-UpperSha256 $OpenVrDllPath
        size = (Get-Item -LiteralPath $OpenVrDllPath).Length
    }
}

$ArtifactIdentity = @($Artifacts | ForEach-Object {
    [ordered]@{
        role = $_.role
        fileName = $_.fileName
        sha256 = $_.sha256
        size = $_.size
    }
})

$Identity = [ordered]@{
    schemaVersion = 1
    manifestType = "cojvr-build"
    source = [ordered]@{
        headCommit = $HeadCommit
        headTree = $HeadTree
        branch = $Branch
        dirty = $Dirty
        reproducibleFromCommit = -not $Dirty
        statusPorcelain = @($StatusLines)
        trackedDiffSha256 = Get-TextSha256 $DiffText
        untrackedFiles = @($Untracked)
    }
    build = [ordered]@{
        configuration = $Configuration
        platform = "Win32"
        configurePreset = "win32-debug"
        diagnosticMode = $DiagnosticMode
    }
    artifacts = @($ArtifactIdentity)
    rebuild = [ordered]@{
        commands = @(
            "tools/bootstrap_openvr.ps1",
            "tools/bootstrap_openxr.ps1",
            "cmake --preset win32-debug",
            "cmake --build --preset $($Configuration.ToLowerInvariant())"
        )
    }
}
$IdentityJson = $Identity | ConvertTo-Json -Depth 10 -Compress
$ManifestId = Get-TextSha256 $IdentityJson

$Manifest = [ordered]@{
    schemaVersion = 1
    manifestType = "cojvr-build"
    manifestId = $ManifestId
    createdUtc = [DateTime]::UtcNow.ToString("o")
    repositoryRoot = $RepositoryRoot
    source = $Identity.source
    build = $Identity.build
    artifacts = $Identity.artifacts
    rebuild = $Identity.rebuild
}

$OutputDirectory = [System.IO.Path]::GetDirectoryName($OutputPath)
if (-not (Test-Path -LiteralPath $OutputDirectory -PathType Container)) {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
}
$Json = $Manifest | ConvertTo-Json -Depth 10
[System.IO.File]::WriteAllText(
    $OutputPath,
    $Json + [Environment]::NewLine,
    [System.Text.UTF8Encoding]::new($false))

Write-Host "Build manifest: $OutputPath"
Write-Host "Manifest ID: $ManifestId"
Write-Host "Source: $HeadCommit (dirty=$($Dirty.ToString().ToLowerInvariant()))"
Write-Host "Proxy SHA-256: $($Artifacts[0].sha256)"
