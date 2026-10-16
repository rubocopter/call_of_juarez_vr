# Host-only verification of extracted release bytes. No game/SteamVR operations.
[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$ZipPath)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Assert-True([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Assert-RelativePath([string]$Path) {
    Assert-True ($Path -cmatch '^[A-Za-z0-9_.+/-]+$' -and $Path -cnotmatch '(^/|//|(^|/)\.{1,2}(/|$)|/$)') "Unsafe package path: $Path"
}
function Assert-ReleaseInventory([string]$Root) {
    $Manifest = Get-Content -LiteralPath (Join-Path $Root 'release-manifest.json') -Raw | ConvertFrom-Json
    Assert-True ($Manifest.schemaVersion -eq 1 -and $Manifest.manifestType -ceq 'cojvr-release') 'Invalid release schema.'
    Assert-True ($Manifest.version -cmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)-prealpha\.(0|[1-9][0-9]*)$') 'Release must use the frontend prealpha.N version format.'
    Assert-True ($Manifest.sourceCommit -cmatch '^[a-f0-9]{40}$') 'Invalid source identity.'
    $Seen = @{}
    foreach ($Item in $Manifest.files) {
        $Name = [string]$Item.path
        Assert-RelativePath $Name
        Assert-True ($Name -cne 'release-manifest.json' -and -not $Seen.ContainsKey($Name)) 'Duplicate or self-referencing inventory item.'
        $Seen[$Name] = $true
        Assert-True ($Item.sha256 -cmatch '^[A-F0-9]{64}$') "Invalid SHA-256: $Name"
        Assert-True (($Item.size -is [int] -or $Item.size -is [long]) -and $Item.size -ge 0) "Invalid size: $Name"
        $Path = Join-Path $Root $Name
        Assert-True (Test-Path -LiteralPath $Path -PathType Leaf) "Missing file: $Name"
        Assert-True ((Get-Item -LiteralPath $Path).Length -eq $Item.size) "Size mismatch: $Name"
        Assert-True ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ceq $Item.sha256) "Hash mismatch: $Name"
    }
    $Actual = @(Get-ChildItem -LiteralPath $Root -File -Recurse)
    Assert-True ($Actual.Count -eq $Seen.Count + 1) 'Package contains uninventoried files.'
    foreach ($File in $Actual) {
        $Name = $File.FullName.Substring($Root.Length + 1).Replace('\', '/')
        Assert-True ($Name -ceq 'release-manifest.json' -or $Seen.ContainsKey($Name)) "Unexpected file: $Name"
    }
    return $Manifest
}
function Assert-Machine([string]$Path, [int]$Machine) {
    $Bytes = [IO.File]::ReadAllBytes($Path)
    Assert-True ($Bytes.Length -gt 64 -and [BitConverter]::ToUInt16($Bytes, 0) -eq 0x5A4D) "Not a PE image: $Path"
    $Pe = [BitConverter]::ToInt32($Bytes, 0x3C)
    Assert-True ($Pe -ge 64 -and $Pe + 6 -le $Bytes.Length -and [BitConverter]::ToUInt32($Bytes, $Pe) -eq 0x4550) 'Invalid PE header.'
    Assert-True ([BitConverter]::ToUInt16($Bytes, $Pe + 4) -eq $Machine) "Wrong PE architecture: $Path"
}
function Assert-Rejects([scriptblock]$Action, [string]$Message) {
    $Rejected = $false
    try { & $Action | Out-Null } catch { $Rejected = $true }
    Assert-True $Rejected $Message
}
function Read-PlayerRelease([string]$Root) {
    # Run the shipped implementation in a child scope, preserving this verifier
    # as an independent oracle while avoiding shared mutable helper definitions.
    & {
        . (Join-Path $Root 'release_common.ps1')
        Read-CojvrRelease $Root
    }
}

$ZipPath = [IO.Path]::GetFullPath($ZipPath)
$Temporary = Join-Path ([IO.Path]::GetTempPath()) ('cojvr-package-' + [Guid]::NewGuid().ToString('N'))
# Intentionally relocate into a path containing spaces and Unicode.
$Root = Join-Path $Temporary ('Offline package ' + [char]0x00E1)
New-Item -ItemType Directory -Path $Root -Force | Out-Null
try {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $Zip = [IO.Compression.ZipFile]::OpenRead($ZipPath)
    try {
        $Seen = @{}
        foreach ($Entry in $Zip.Entries) {
            Assert-RelativePath $Entry.FullName
            Assert-True (-not $Seen.ContainsKey($Entry.FullName)) 'Duplicate ZIP entry.'
            $Seen[$Entry.FullName] = $true
            Assert-True ($Entry.LastWriteTime.Year -eq 1980 -and $Entry.LastWriteTime.Month -eq 1 -and $Entry.LastWriteTime.Day -eq 1) 'Non-deterministic ZIP timestamp.'
        }
    } finally { $Zip.Dispose() }
    [IO.Compression.ZipFile]::ExtractToDirectory($ZipPath, $Root)
    $Manifest = Assert-ReleaseInventory $Root
    $PlayerManifest = Read-PlayerRelease $Root
    Assert-True ($PlayerManifest.version -ceq $Manifest.version -and $PlayerManifest.files.Count -eq $Manifest.files.Count) 'Shipped validator and independent inventory disagree.'
    foreach ($Required in @('cojvr.ps1', 'CoJ-VR.cmd', 'dependencies.lock.json', 'THIRD_PARTY_NOTICES.txt',
        'licenses/OpenVR-LICENSE.txt', 'payload/d3d9_native_stereo.dll', 'payload/openvr_api.dll',
        'payload/d3d9_native_stereo.build-manifest.json', 'payload/cojvr_openvr_input/actions.json',
        'payload/cojvr_openvr_input/bindings/psvr2_sense.json', 'dependencies/python/python.exe',
        'payload/cojvr_openvr_input/bindings/knuckles.json', 'payload/cojvr_openvr_input/bindings/vive_controller.json',
        'payload/cojvr_openvr_input/bindings/oculus_touch.json', 'payload/cojvr_openvr_input/bindings/holographic_controller.json',
        'dependencies/python/python313._pth', 'dependencies/python/LICENSE.txt', 'dependencies/vc_redist.x86.exe')) {
        Assert-True (Test-Path -LiteralPath (Join-Path $Root $Required) -PathType Leaf) "Required release file missing: $Required"
    }
    $ExpectedTools = @('deployment_transaction.ps1', 'stage_d3d9_proxy.ps1', 'unstage_d3d9_proxy.ps1', 'openvr_input_manifest.ps1',
        'set_hmd_camera_control.ps1', 'coj_video_profile.ps1', 'patch_coj_weapon_consumers.py',
        'inspect_java_bytecode.py', 'coj_reload_wait_probe.py', 'coj_manual_reload.py',
        'coj_physical_punch.py', 'split_coj_player_mesh.py', 'patch_coj_large_address.py')
    $ActualTools = @(Get-ChildItem -LiteralPath (Join-Path $Root 'tools') -File -Recurse | ForEach-Object { $_.Name })
    Assert-True (@(Compare-Object $ExpectedTools $ActualTools).Count -eq 0) 'Release tools differ from the explicit allowlist.'
    foreach ($Item in $Manifest.files) {
        Assert-True ($Item.path -notmatch '(?i)(\.pak$|\.pdb$|\.lib$|\.obj$|\.pyc$|(^|/)(__pycache__|work|build|\.git)/)') "Forbidden player payload: $($Item.path)"
        Assert-True ($Item.path -notmatch '\.exe$' -or $Item.path -cmatch '^dependencies/(vc_redist\.x86\.exe|python/pythonw?\.exe)$') 'Unexpected executable in player ZIP.'
    }
    $Lock = Get-Content -LiteralPath (Join-Path $Root 'dependencies.lock.json') -Raw | ConvertFrom-Json
    Assert-True ($Manifest.dependencies.pythonVersion -ceq $Lock.python.version -and
        $Manifest.dependencies.vcRuntimeVersion -ceq $Lock.vcRuntime.version) 'Release dependencies differ from lock.'
    $Vc = Join-Path $Root 'dependencies/vc_redist.x86.exe'
    Assert-True ((Get-FileHash -LiteralPath $Vc -Algorithm SHA256).Hash -ceq $Lock.vcRuntime.sha256) 'VC installer differs from lock.'
    $Signature = Get-AuthenticodeSignature -LiteralPath $Vc
    Assert-True ($Signature.Status -eq 'Valid' -and $Signature.SignerCertificate.Subject -ceq $Lock.vcRuntime.authenticodeSubject) 'VC installer signature is not valid Microsoft.'
    $BuildText = [IO.File]::ReadAllText((Join-Path $Root 'payload/d3d9_native_stereo.build-manifest.json'))
    $Build = $BuildText | ConvertFrom-Json
    Assert-True ($Build.manifestType -ceq 'cojvr-build' -and $Build.schemaVersion -eq 1 -and
        $Build.source.headCommit -ceq $Manifest.sourceCommit -and $Build.build.configuration -ceq 'Release' -and
        $Build.build.platform -ceq 'Win32') 'Portable build identity mismatch.'
    Assert-True ($BuildText -notmatch '(?i)(repositoryRoot|sourcePath|createdUtc|rebuild|[a-z]:\\|\\\\[a-z])') 'Portable manifest leaks local provenance.'
    $BuildFiles = @{}
    foreach ($Artifact in $Build.artifacts) {
        Assert-RelativePath $Artifact.fileName
        Assert-True (-not $BuildFiles.ContainsKey($Artifact.fileName)) 'Duplicate build inventory entry.'
        $BuildFiles[$Artifact.fileName] = $true
        $Path = Join-Path $Root ('payload/' + $Artifact.fileName)
        Assert-True ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ceq $Artifact.sha256 -and
            (Get-Item -LiteralPath $Path).Length -eq $Artifact.size) 'Portable build artifact mismatch.'
    }
    foreach ($InputFile in Get-ChildItem -LiteralPath (Join-Path $Root 'payload/cojvr_openvr_input') -Recurse -File) {
        Assert-True ($BuildFiles.ContainsKey($InputFile.FullName.Substring((Join-Path $Root 'payload').Length + 1).Replace('\', '/'))) 'Unbound controller input asset.'
    }
    Assert-True ((Get-FileHash -LiteralPath (Join-Path $Root 'payload/openvr_api.dll') -Algorithm SHA256).Hash -ceq $Lock.openvr.win32DllSha256) 'Unpinned OpenVR DLL.'
    Assert-True ((Get-FileHash -LiteralPath (Join-Path $Root 'licenses/OpenVR-LICENSE.txt') -Algorithm SHA256).Hash -ceq $Lock.openvr.licenseSha256) 'OpenVR license differs from pinned SDK.'
    Assert-Machine (Join-Path $Root 'payload/d3d9_native_stereo.dll') 0x14C
    Assert-Machine (Join-Path $Root 'payload/openvr_api.dll') 0x14C
    Assert-Machine (Join-Path $Root 'dependencies/python/python.exe') 0x8664
    $Ascii = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes((Join-Path $Root 'payload/d3d9_native_stereo.dll')))
    foreach ($Match in [regex]::Matches($Ascii, 'RSDS[\s\S]{20}([^\x00]+)\x00')) {
        Assert-True ($Match.Groups[1].Value -notmatch '[:\\/]') 'DLL leaks an absolute PDB path.'
    }
    Assert-True ((Get-Content -LiteralPath (Join-Path $Root 'dependencies/python/python313._pth') -Raw).Trim() -ceq "python313.zip`n.`n../../tools") 'Isolated Python import path is incorrect.'
    foreach ($Script in Get-ChildItem -LiteralPath $Root -Filter '*.ps1' -Recurse -File) {
        $ParseErrors = $null; $Tokens = $null
        [void][Management.Automation.Language.Parser]::ParseFile($Script.FullName, [ref]$Tokens, [ref]$ParseErrors)
        Assert-True ($ParseErrors.Count -eq 0) "PowerShell parse failure: $($Script.Name)"
    }
    Push-Location $Temporary
    try {
        $Python = Join-Path $Root 'dependencies/python/python.exe'
        $PythonVersion = & $Python -B -c 'import sys; print(sys.version.split()[0]); assert sys.flags.isolated == 1'
        Assert-True ($LASTEXITCODE -eq 0 -and $PythonVersion -ceq $Lock.python.version) 'Bundled isolated Python failed.'
        & $Python -B -c 'import patch_coj_weapon_consumers, inspect_java_bytecode, coj_reload_wait_probe, coj_manual_reload, coj_physical_punch, split_coj_player_mesh, patch_coj_large_address'
        Assert-True ($LASTEXITCODE -eq 0) 'Relocated offline patcher imports failed.'
    } finally { Pop-Location }

    # Exercise the actual PS5.1 frontend's read-only path on the relocated ZIP.
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'cojvr.ps1') -Action Verify
    Assert-True ($LASTEXITCODE -eq 0) 'Extracted frontend verification failed.'
    & {
        . (Join-Path $Root 'tools/openvr_input_manifest.ps1')
        Assert-CojvrOpenVrInputManifest $Build.openVrInputManifest
        $Inputs = Get-CojvrOpenVrInputManifest (Join-Path $Root 'payload/cojvr_openvr_input')
        Assert-True (($Inputs | ConvertTo-Json -Depth 10 -Compress) -ceq ($Build.openVrInputManifest | ConvertTo-Json -Depth 10 -Compress)) 'Shared staging input inventory differs after relocation.'
    }

    # Exercise the verifier against real extracted-file corruption, omissions,
    # additions and malicious inventory paths. Restore before the final full check.
    $Probe = Join-Path $Root 'CoJ-VR.cmd'
    $Original = [IO.File]::ReadAllBytes($Probe)
    [IO.File]::AppendAllText($Probe, 'tamper')
    Assert-Rejects { Assert-ReleaseInventory $Root } 'Tampered bytes were accepted.'
    Assert-Rejects { Read-PlayerRelease $Root } 'Shipped validator accepted tampered bytes.'
    [IO.File]::WriteAllBytes($Probe, $Original)
    Move-Item -LiteralPath $Probe -Destination (Join-Path $Temporary 'launcher.saved')
    Assert-Rejects { Assert-ReleaseInventory $Root } 'Missing inventory file was accepted.'
    Assert-Rejects { Read-PlayerRelease $Root } 'Shipped validator accepted a missing file.'
    Move-Item -LiteralPath (Join-Path $Temporary 'launcher.saved') -Destination $Probe
    [IO.File]::WriteAllText((Join-Path $Root 'unexpected.txt'), 'unexpected')
    Assert-Rejects { Assert-ReleaseInventory $Root } 'Uninventoried file was accepted.'
    Assert-Rejects { Read-PlayerRelease $Root } 'Shipped validator accepted an extra file.'
    Remove-Item -LiteralPath (Join-Path $Root 'unexpected.txt')
    $ManifestPath = Join-Path $Root 'release-manifest.json'
    $OriginalManifest = [IO.File]::ReadAllBytes($ManifestPath)
    $Manifest.files[0].path = '../outside.txt'
    [IO.File]::WriteAllText($ManifestPath, ($Manifest | ConvertTo-Json -Depth 30))
    Assert-Rejects { Assert-ReleaseInventory $Root } 'Traversal inventory was accepted.'
    Assert-Rejects { Read-PlayerRelease $Root } 'Shipped validator accepted traversal inventory.'
    [IO.File]::WriteAllBytes($ManifestPath, $OriginalManifest)
    $Final = Assert-ReleaseInventory $Root
    Write-Host "Release package verified: $($Final.files.Count) inventoried files; extracted hashes, x86 payload, x64 isolated Python imports, Microsoft signature, tamper rejection. Host-only; no headset validation."
} finally {
    if (Test-Path -LiteralPath $Temporary) {
        if (-not ([IO.Path]::GetFullPath($Temporary)).StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()), [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe test temporary path.' }
        Remove-Item -LiteralPath $Temporary -Recurse -Force
    }
}
