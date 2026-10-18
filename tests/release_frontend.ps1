param([string]$SourceDirectory = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$Helper = Join-Path $SourceDirectory 'release/release_common.ps1'
if (-not (Test-Path -LiteralPath $Helper)) { throw 'Release package validation is not implemented.' }
. $Helper
$Root = Join-Path $SourceDirectory ('work/release-tests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $Root -Force | Out-Null
function Assert-True($Value, $Message) { if (-not $Value) { throw $Message } }
function Assert-Rejects([scriptblock]$Operation, [string]$Message) {
    $Rejected = $false
    try { & $Operation | Out-Null } catch { $Rejected = $true }
    Assert-True $Rejected $Message
}
function New-Package {
    $Package = Join-Path $Root ([guid]::NewGuid().ToString('N') + ' paquete')
    New-Item -ItemType Directory -Path $Package | Out-Null
    $Names = @('cojvr.ps1','release_common.ps1','CoJ-VR.cmd',
        'payload/d3d9_native_stereo.dll','payload/openvr_api.dll',
        'payload/d3d9_native_stereo.build-manifest.json','payload/cojvr_openvr_input/actions.json',
        'dependencies/python/python.exe','dependencies/vc_redist.x86.exe',
        'tools/stage_d3d9_proxy.ps1','tools/unstage_d3d9_proxy.ps1',
        'tools/deployment_transaction.ps1','tools/coj_video_profile.ps1','tools/set_hmd_camera_control.ps1',
        'tools/coj_resolution_settings.ps1','tools/coj_resolution_session.ps1','tools/coj_resolution_profiles.py',
        'tools/cojvr_openvr_resolution_query.exe','tools/cojvr_d3d9_resolution_limits.exe','tools/openvr_api.dll')
    $Files = @(foreach ($Name in $Names) {
        $Path = Join-Path $Package $Name
        New-Item -ItemType Directory -Path (Split-Path $Path -Parent) -Force | Out-Null
        [IO.File]::WriteAllText($Path, 'fixture-' + $Name)
        @{path=$Name;sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash;size=(Get-Item -LiteralPath $Path).Length}
    })
    $Manifest = @{schemaVersion=1;manifestType='cojvr-release';version='0.1.0-prealpha.1';sourceCommit=('a'*40);files=$Files}
    Write-CojvrPublicJson (Join-Path $Package 'release-manifest.json') $Manifest
    return $Package
}
try {
    $Package = New-Package
    Assert-True ((Read-CojvrRelease $Package).version -eq '0.1.0-prealpha.1') 'Valid relocated package rejected.'
    [IO.File]::AppendAllText((Join-Path $Package 'payload/openvr_api.dll'), 'tampered')
    Assert-Rejects { Read-CojvrRelease $Package } 'Tampered runtime accepted.'
    $Package = New-Package
    $Manifest = Get-Content -LiteralPath (Join-Path $Package 'release-manifest.json') -Raw | ConvertFrom-Json
    $Manifest.files[0].path = '../outside.ps1'
    Write-CojvrPublicJson (Join-Path $Package 'release-manifest.json') $Manifest
    Assert-Rejects { Read-CojvrRelease $Package } 'Traversal accepted.'
    $Package = New-Package
    $Manifest = Get-Content -LiteralPath (Join-Path $Package 'release-manifest.json') -Raw | ConvertFrom-Json
    $Manifest.files += $Manifest.files[0]
    Write-CojvrPublicJson (Join-Path $Package 'release-manifest.json') $Manifest
    Assert-Rejects { Read-CojvrRelease $Package } 'Duplicate inventory accepted.'
    $Package = New-Package
    [IO.File]::WriteAllText((Join-Path $Package 'foreign.dll'), 'unexpected')
    Assert-Rejects { Read-CojvrRelease $Package } 'Uninventoried file accepted.'
    $Game = Join-Path $Root 'unknown game'
    New-Item -ItemType Directory -Path $Game | Out-Null
    [IO.File]::WriteAllText((Join-Path $Game 'CoJ.exe'), 'unknown')
    Assert-Rejects { Assert-CojvrPublicGame $Game } 'Unknown game accepted.'
    Assert-True (@(Get-ChildItem -LiteralPath $Game -Force).Count -eq 1) 'Unknown game preflight mutated files.'
    [IO.File]::WriteAllText((Join-Path $Game '.cojvr-d3d9-stage.json'), '{}')
    Assert-Rejects { Assert-CojvrPublicOwnership $Game } 'Foreign development staging accepted.'
    Write-CojvrPublicJson (Join-Path $Game '.cojvr-public-install.json') @{schemaVersion=1;manifestType='cojvr-public-install';installId=('a'*32);phase='unknown';buildManifestId='../outside'}
    Assert-Rejects { Assert-CojvrPublicOwnership $Game } 'Invalid public recovery identity accepted.'
    [IO.File]::WriteAllText((Join-Path $Game 'cojvr.log'), ('log ' + $env:USERPROFILE + '\private'))
    [IO.File]::WriteAllText((Join-Path $Game 'secret.sav'), 'never include saves')
    [IO.File]::WriteAllText((Join-Path $Game '.cojvr-run.json'), '{"runId":"report-run","buildManifestId":"report-build"}')
    [IO.File]::WriteAllText((Join-Path $Game 'cojvr-render-fault-123.txt'), "run_id=report-run`nbuild_manifest_id=report-build`n")
    [IO.File]::WriteAllText((Join-Path $Game 'cojvr-render-fault-999.txt'), "run_id=stale-run`nbuild_manifest_id=report-build`n")
    $Reports = Join-Path $Root 'reports'
    $Zip = New-CojvrPublicReport -GameDirectory $Game -OutputDirectory $Reports -Version 'test'
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $Archive = [IO.Compression.ZipFile]::OpenRead($Zip)
    try {
        Assert-True (@($Archive.Entries | Where-Object { $_.FullName -like '*.sav' }).Count -eq 0) 'Report leaked save.'
        Assert-True ($null -ne $Archive.GetEntry('cojvr-render-fault-123.txt')) 'Current native fault report omitted.'
        Assert-True ($null -eq $Archive.GetEntry('cojvr-render-fault-999.txt')) 'Stale fault report included.'
        $Entry = $Archive.GetEntry('cojvr.log')
        Assert-True ($null -ne $Entry) 'Report omitted log.'
        $Reader = [IO.StreamReader]::new($Entry.Open())
        try { $Text = $Reader.ReadToEnd() } finally { $Reader.Dispose() }
        Assert-True (-not $Text.Contains($env:USERPROFILE)) 'Report leaked user-profile path.'
    } finally { $Archive.Dispose() }
    Write-Host 'PASS release frontend: inventory, relocation, tampering, traversal, duplicates, unknown game, ownership and private diagnostics.'
    Assert-True (Test-Path -LiteralPath (Join-Path $SourceDirectory 'release/cojvr.ps1')) 'Public installer command is not implemented.'
    $Package = New-Package
    foreach ($Relative in @('release/cojvr.ps1','release/release_common.ps1','tools/unstage_d3d9_proxy.ps1','tools/deployment_transaction.ps1','tools/coj_video_profile.ps1','tools/openvr_input_manifest.ps1','tools/coj_resolution_settings.ps1')) {
        $Destination = if ($Relative.StartsWith('release/')) { $Relative.Substring(8) } else { $Relative }
        Copy-Item -LiteralPath (Join-Path $SourceDirectory $Relative) -Destination (Join-Path $Package $Destination) -Force
    }
    $Manifest = Get-Content -LiteralPath (Join-Path $Package 'release-manifest.json') -Raw | ConvertFrom-Json
    $Manifest.files = @(Get-ChildItem -LiteralPath $Package -File -Recurse | Where-Object { $_.Name -ne 'release-manifest.json' } | ForEach-Object {
        @{path=$_.FullName.Substring($Package.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash;size=$_.Length}
    })
    Write-CojvrPublicJson (Join-Path $Package 'release-manifest.json') $Manifest
    $Preference=Join-Path $Root 'user/resolution.json'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Package 'cojvr.ps1') -Action ConfigureResolution -ResolutionProfile Manual -ResolutionWidth 2804 -ResolutionHeight 2860 -ResolutionSettingsPath $Preference
    Assert-True ($LASTEXITCODE -eq 0 -and (Get-Content $Preference -Raw | ConvertFrom-Json).width -eq 2804) 'Relocated selector did not persist the Manual choice.'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Package 'cojvr.ps1') -Action ConfigureResolution -ResolutionSettingsPath $Preference
    Assert-True ($LASTEXITCODE -eq 0 -and (Get-Content $Preference -Raw | ConvertFrom-Json).profile -eq 'Auto') 'Default selector action did not choose automatic headset resolution.'
    . (Join-Path $SourceDirectory 'tools/deployment_transaction.ps1')
    $Public = @{schemaVersion=1;manifestType='cojvr-public-install';installId=('a'*32);phase='installing';buildManifestId=('b'*64);sourceCommit=('c'*40);version='0.1.0-prealpha.1';runId='public-recovery-test'}
    Write-CojvrPublicJson (Join-Path $Game '.cojvr-public-install.json') $Public
    [IO.File]::WriteAllText((Join-Path $Game '.cojvr-d3d9-stage.json'), '{"schemaVersion":')
    [IO.File]::WriteAllText((Join-Path $Game 'd3d9.dll'), 'staged-for-recovery')
    $Assets = @(@{role='proxy';kind='file';destination='d3d9.dll';backup='d3d9.cojvr-backup.dll';temporary='d3d9.cojvr-installing.dll';hadOriginal=$false;stagedSha256=(Get-FileHash -LiteralPath (Join-Path $Game 'd3d9.dll') -Algorithm SHA256).Hash})
    [void](Write-CojvrDeploymentJournal $Game 'stage' $Public.runId 'd3d9_native_stereo' $Assets)
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Package 'cojvr.ps1') -Action Uninstall -GameDirectory $Game
    Assert-True ($LASTEXITCODE -eq 0) 'Public recovery rejected a valid owned journal because stage JSON was truncated.'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $Game 'd3d9.dll'))) 'Recovery left partial proxy installed.'
} finally {
    $Resolved = [IO.Path]::GetFullPath($Root)
    $Allowed = [IO.Path]::GetFullPath((Join-Path $SourceDirectory 'work')).TrimEnd('\') + '\'
    if ($Resolved.StartsWith($Allowed, [StringComparison]::OrdinalIgnoreCase)) { Remove-Item -LiteralPath $Resolved -Recurse -Force }
}
