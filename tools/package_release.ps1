# Creates a portable offline player ZIP. This script never stages or runs the game.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [string]$BuildDirectory = 'build/win32-public-release',
    [string]$OutputDirectory = 'dist',
    [string]$DependencyDirectory = '.cache/release-deps',
    [switch]$SkipBuild,
    [switch]$AllowDirty
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
function Resolve-RepositoryPath([string]$Path) {
    if (-not [IO.Path]::IsPathRooted($Path)) { $Path = Join-Path $RepositoryRoot $Path }
    return [IO.Path]::GetFullPath($Path)
}
function Invoke-Git([string[]]$Arguments) {
    $Result = & git -C $RepositoryRoot @Arguments
    if ($LASTEXITCODE -ne 0) { throw "git $($Arguments -join ' ') failed." }
    return $Result
}
function Write-Json([string]$Path, $Value) {
    [IO.File]::WriteAllText($Path, (($Value | ConvertTo-Json -Depth 30) + "`n"), [Text.UTF8Encoding]::new($false))
}
function Get-Hash([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant() }
function Assert-CleanSource {
    if (-not $AllowDirty -and @(Invoke-Git @('status', '--porcelain=v1', '--untracked-files=all')).Count -ne 0) {
        throw 'Public packaging requires a clean source tree. -AllowDirty is for local smoke packages only.'
    }
}
# Match the player frontend's prealpha.N contract; numeric semver parts cannot
# have leading zeroes. A leading v belongs only to the Git tag, not this value.
if ($Version -cnotmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)-prealpha\.(0|[1-9][0-9]*)$') {
    throw 'Version must use the semantic prerelease form 0.1.0-prealpha.1 (without a leading v).'
}
Assert-CleanSource
$SourceCommit = [string](Invoke-Git @('rev-parse', 'HEAD'))
$BuildDirectory = Resolve-RepositoryPath $BuildDirectory
$OutputDirectory = Resolve-RepositoryPath $OutputDirectory
$DependencyDirectory = Resolve-RepositoryPath $DependencyDirectory
$Artifacts = Join-Path $BuildDirectory 'Release'
$BuildManifestPath = Join-Path $Artifacts 'd3d9_native_stereo.build-manifest.json'
$ProxyPath = Join-Path $Artifacts 'd3d9_native_stereo.dll'

if (-not $SkipBuild) {
    # OpenVR is the public path. Keep symbols, but embed only their basename in
    # the DLL's CodeView record; developer PDBs are uploaded separately by CI.
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'bootstrap_openvr.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'OpenVR bootstrap failed.' }
    & cmake -S $RepositoryRoot -B $BuildDirectory -G 'Visual Studio 17 2022' -A Win32 `
        -DCOJVR_BUILD_TESTS=ON -DCOJVR_ENABLE_OPENVR=ON -DCOJVR_ENABLE_OPENXR=OFF `
        '-DCMAKE_SHARED_LINKER_FLAGS_RELEASE=/INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF /PDBALTPATH:%_PDB%'
    if ($LASTEXITCODE -ne 0) { throw 'Release configure failed.' }
    & cmake --build $BuildDirectory --config Release --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Release build failed.' }
    & ctest --test-dir $BuildDirectory -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Release host tests failed.' }
    & (Join-Path $PSScriptRoot 'new_build_manifest.ps1') -RepositoryRoot $RepositoryRoot `
        -Configuration Release -DiagnosticMode d3d9_native_stereo -ProxyPath $ProxyPath `
        -OutputPath $BuildManifestPath -AllowDirty:$AllowDirty
}
# SkipBuild consumes existing provenance; it must never relabel stale output as HEAD.
$BuildManifest = Get-Content -LiteralPath $BuildManifestPath -Raw | ConvertFrom-Json
if ($BuildManifest.schemaVersion -ne 1 -or $BuildManifest.manifestType -ne 'cojvr-build' -or
    $BuildManifest.build.configuration -ne 'Release' -or $BuildManifest.build.platform -ne 'Win32' -or
    $BuildManifest.build.diagnosticMode -ne 'd3d9_native_stereo' -or
    $BuildManifest.source.headCommit -cne $SourceCommit) { throw 'Build provenance does not match this Release/Win32/OpenVR source commit.' }
if (-not $AllowDirty -and ($BuildManifest.source.dirty -or -not $BuildManifest.source.reproducibleFromCommit)) {
    throw 'Public package cannot consume a dirty build manifest.'
}
if ($BuildManifest.manifestId -cnotmatch '^[A-F0-9]{64}$') { throw 'Invalid build identity.' }
foreach ($RequiredRole in @('proxy', 'openvr_runtime', 'openvr_action_manifest', 'openvr_binding_psvr2_sense')) {
    if (@($BuildManifest.artifacts | Where-Object { $_.role -eq $RequiredRole }).Count -ne 1) { throw "Build inventory must contain exactly one $RequiredRole." }
}
foreach ($Name in @('cojvr_openvr_resolution_query.exe','cojvr_d3d9_resolution_limits.exe')) {
    if (@($BuildManifest.artifacts | Where-Object { $_.role -eq 'resolution_helper' -and $_.fileName -eq $Name }).Count -ne 1) {
        throw "Build provenance omits resolution helper: $Name. Build helpers and regenerate the manifest."
    }
}
$ArtifactNames = @{}
foreach ($Artifact in $BuildManifest.artifacts) {
    $Name = [string]$Artifact.fileName
    if ($Name -notin @('d3d9_native_stereo.dll', 'openvr_api.dll','cojvr_openvr_resolution_query.exe','cojvr_d3d9_resolution_limits.exe') -and $Name -cnotmatch '^cojvr_openvr_input/[A-Za-z0-9_./-]+\.json$') {
        throw "Unexpected release build artifact: $Name"
    }
    if ($Name -match '(^|/)\.\.(/|$)' -or $ArtifactNames.ContainsKey($Name)) { throw 'Unsafe or duplicate build artifact.' }
    $ArtifactNames[$Name] = $true
    $Path = Join-Path $Artifacts $Name
    if ((Get-Hash $Path) -cne $Artifact.sha256 -or (Get-Item -LiteralPath $Path).Length -ne $Artifact.size) { throw "Build artifact differs from provenance: $Name" }
}
$Lock = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'release/dependencies.lock.json') -Raw | ConvertFrom-Json
if ((Get-Hash (Join-Path $Artifacts 'openvr_api.dll')) -cne $Lock.openvr.win32DllSha256) { throw 'Build uses an unpinned OpenVR DLL.' }
if ((Get-Hash (Join-Path $RepositoryRoot '.cache/deps/openvr-2.15.6/LICENSE')) -cne $Lock.openvr.licenseSha256) { throw 'OpenVR license differs from the pinned SDK.' }
# Reject absolute PDB paths even with SkipBuild. Never mutate the signed/hash-bound DLL.
$ProxyBytes = [IO.File]::ReadAllBytes($ProxyPath)
$ProxyAscii = [Text.Encoding]::ASCII.GetString($ProxyBytes)
foreach ($Match in [regex]::Matches($ProxyAscii, 'RSDS[\s\S]{20}([^\x00]+)\x00')) {
    if ($Match.Groups[1].Value -match '[:\\/]') { throw 'DLL embeds a local PDB path. Rebuild with package_release.ps1 (without -SkipBuild).' }
}

& (Join-Path $PSScriptRoot 'bootstrap_release_dependencies.ps1') -Destination $DependencyDirectory
$Tools = @('deployment_transaction.ps1', 'stage_d3d9_proxy.ps1', 'unstage_d3d9_proxy.ps1', 'openvr_input_manifest.ps1',
    'set_hmd_camera_control.ps1', 'coj_video_profile.ps1', 'coj_resolution_profiles.py',
    'coj_resolution_settings.ps1','coj_resolution_session.ps1','patch_coj_weapon_consumers.py',
    'inspect_java_bytecode.py', 'coj_reload_wait_probe.py', 'coj_manual_reload.py',
    'coj_physical_punch.py', 'split_coj_player_mesh.py', 'patch_coj_large_address.py')
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$Temporary = Join-Path $OutputDirectory ('.package-' + [Guid]::NewGuid().ToString('N'))
$PackageRoot = Join-Path $Temporary 'player'
$ZipName = "CoJ-VR-$Version-windows-x64.zip"
$ZipPath = Join-Path $OutputDirectory $ZipName
if ((Test-Path -LiteralPath $ZipPath) -or (Test-Path -LiteralPath "$ZipPath.sha256")) { throw 'Release output already exists; choose a new output directory or version.' }
try {
    foreach ($Directory in @('', 'payload', 'tools', 'dependencies', 'licenses')) {
        New-Item -ItemType Directory -Force -Path (Join-Path $PackageRoot $Directory) | Out-Null
    }
    foreach ($Name in @('cojvr.ps1', 'CoJ-VR.cmd')) {
        Copy-Item -LiteralPath (Join-Path $RepositoryRoot "release/$Name") -Destination (Join-Path $PackageRoot $Name)
    }
    foreach ($Name in @('release_common.ps1', 'README.txt', 'RELEASE_NOTES.md')) {
        $Source = Join-Path $RepositoryRoot "release/$Name"
        if (Test-Path -LiteralPath $Source -PathType Leaf) { Copy-Item -LiteralPath $Source -Destination (Join-Path $PackageRoot $Name) }
    }
    foreach ($Name in $Tools) {
        Copy-Item -LiteralPath (Join-Path $RepositoryRoot "tools/$Name") -Destination (Join-Path $PackageRoot "tools/$Name")
    }
    foreach ($Name in @('cojvr_openvr_resolution_query.exe','cojvr_d3d9_resolution_limits.exe','openvr_api.dll')) {
        Copy-Item -LiteralPath (Join-Path $Artifacts $Name) -Destination (Join-Path $PackageRoot "tools/$Name")
    }
    foreach ($Name in @('d3d9_native_stereo.dll', 'openvr_api.dll', 'cojvr_openvr_input')) {
        Copy-Item -LiteralPath (Join-Path $Artifacts $Name) -Destination (Join-Path $PackageRoot 'payload') -Recurse
    }
    # Every input JSON must be bound by the selected build manifest, including
    # controller bindings added after the original Sense-only inventory.
    foreach ($InputFile in Get-ChildItem -LiteralPath (Join-Path $PackageRoot 'payload/cojvr_openvr_input') -Recurse -File) {
        $Relative = $InputFile.FullName.Substring((Join-Path $PackageRoot 'payload').Length + 1).Replace('\', '/')
        if (-not $ArtifactNames.ContainsKey($Relative)) { throw "Input asset is absent from build provenance: $Relative" }
    }
    Copy-Item -LiteralPath (Join-Path $DependencyDirectory 'python') -Destination (Join-Path $PackageRoot 'dependencies') -Recurse
    Copy-Item -LiteralPath (Join-Path $DependencyDirectory 'vc_redist.x86.exe') -Destination (Join-Path $PackageRoot 'dependencies/vc_redist.x86.exe')
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'release/dependencies.lock.json') -Destination (Join-Path $PackageRoot 'dependencies.lock.json')
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'release/THIRD_PARTY_NOTICES.txt') -Destination (Join-Path $PackageRoot 'THIRD_PARTY_NOTICES.txt')
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot '.cache/deps/openvr-2.15.6/LICENSE') -Destination (Join-Path $PackageRoot 'licenses/OpenVR-LICENSE.txt')
    # Keep the original manifestId as the build correlation identity. Only local
    # provenance fields (paths, status, rebuild commands, timestamp) are omitted.
    $PortableBuildManifest = [ordered]@{
        schemaVersion = 1; manifestType = 'cojvr-build'; manifestId = $BuildManifest.manifestId
        source = [ordered]@{
            headCommit = $BuildManifest.source.headCommit; headTree = $BuildManifest.source.headTree
            dirty = $BuildManifest.source.dirty; reproducibleFromCommit = $BuildManifest.source.reproducibleFromCommit
            trackedDiffSha256 = $BuildManifest.source.trackedDiffSha256
        }
        build = [ordered]@{ configuration = 'Release'; platform = 'Win32'; diagnosticMode = 'd3d9_native_stereo' }
        openVrInputManifest = $BuildManifest.openVrInputManifest
        artifacts = @($BuildManifest.artifacts | ForEach-Object { [ordered]@{ role = $_.role; fileName = $_.fileName; sha256 = $_.sha256; size = [long]$_.size } })
    }
    Write-Json (Join-Path $PackageRoot 'payload/d3d9_native_stereo.build-manifest.json') $PortableBuildManifest
    # Import every shipped patcher from a foreign working directory using the
    # isolated interpreter. -B keeps bytecode and local caches out of the ZIP.
    Push-Location $Temporary
    try {
        & (Join-Path $PackageRoot 'dependencies/python/python.exe') -B -c 'import patch_coj_weapon_consumers, inspect_java_bytecode, coj_reload_wait_probe, coj_manual_reload, coj_physical_punch, split_coj_player_mesh, patch_coj_large_address'
        if ($LASTEXITCODE -ne 0) { throw 'Offline bundled patcher import check failed.' }
    } finally { Pop-Location }
    Assert-CleanSource
    if ([string](Invoke-Git @('rev-parse', 'HEAD')) -cne $SourceCommit) { throw 'Source HEAD changed during packaging.' }
    $Inventory = @{}
    foreach ($File in Get-ChildItem -LiteralPath $PackageRoot -Recurse -File) {
        if ($File.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Package cannot contain reparse points.' }
        $Name = $File.FullName.Substring($PackageRoot.Length + 1).Replace('\', '/')
        $Inventory[$Name] = [ordered]@{ path = $Name; sha256 = Get-Hash $File.FullName; size = [long]$File.Length }
    }
    [string[]]$Names = @($Inventory.Keys)
    [Array]::Sort($Names, [StringComparer]::Ordinal)
    Write-Json (Join-Path $PackageRoot 'release-manifest.json') ([ordered]@{
        schemaVersion = 1; manifestType = 'cojvr-release'; version = $Version; sourceCommit = $SourceCommit
        files = @($Names | ForEach-Object { $Inventory[$_] })
        dependencies = [ordered]@{ pythonVersion = $Lock.python.version; vcRuntimeVersion = $Lock.vcRuntime.version }
    })
    Add-Type -AssemblyName System.IO.Compression
    $Stream = [IO.File]::Open((Join-Path $Temporary $ZipName), [IO.FileMode]::CreateNew)
    try {
        $Zip = [IO.Compression.ZipArchive]::new($Stream, [IO.Compression.ZipArchiveMode]::Create, $true)
        try {
            [string[]]$ZipNames = @($Names) + @('release-manifest.json')
            [Array]::Sort($ZipNames, [StringComparer]::Ordinal)
            foreach ($Name in $ZipNames) {
                $Entry = $Zip.CreateEntry($Name, [IO.Compression.CompressionLevel]::Optimal)
                $Entry.LastWriteTime = [DateTimeOffset]::new(1980, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
                $InputStream = [IO.File]::OpenRead((Join-Path $PackageRoot $Name))
                $EntryStream = $Entry.Open()
                try { $InputStream.CopyTo($EntryStream) } finally { $EntryStream.Dispose(); $InputStream.Dispose() }
            }
        } finally { $Zip.Dispose() }
    } finally { $Stream.Dispose() }
    # Verify the actual extracted bytes and offline imports before publishing output.
    & (Join-Path $RepositoryRoot 'tests/release_package.ps1') -ZipPath (Join-Path $Temporary $ZipName)
    $ZipHash = Get-Hash (Join-Path $Temporary $ZipName)
    Move-Item -LiteralPath (Join-Path $Temporary $ZipName) -Destination $ZipPath
    [IO.File]::WriteAllText("$ZipPath.sha256", "$ZipHash  $ZipName`n", [Text.Encoding]::ASCII)
    Write-Host "Release ZIP: $ZipPath"
    Write-Host "SHA-256: $ZipHash"
    if ($AllowDirty) { Write-Warning 'Local smoke package: -AllowDirty is not eligible for public release.' }
} finally {
    if (Test-Path -LiteralPath $Temporary) {
        if (-not ([IO.Path]::GetFullPath($Temporary)).StartsWith($OutputDirectory + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe packaging temporary path.' }
        Remove-Item -LiteralPath $Temporary -Recurse -Force
    }
}
