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
    $Names = @('cojvr.ps1','cojvr-gui.ps1','release_common.ps1','CoJ-VR.cmd',
        'payload/d3d9_native_stereo.dll','payload/openvr_api.dll',
        'payload/d3d9_native_stereo.build-manifest.json','payload/cojvr_openvr_input/actions.json',
        'dependencies/python/python.exe','dependencies/vc_redist.x86.exe',
        'tools/stage_d3d9_proxy.ps1','tools/unstage_d3d9_proxy.ps1',
        'tools/coj_executable_identity.ps1','tools/deployment_transaction.ps1','tools/coj_video_profile.ps1','tools/set_hmd_camera_control.ps1',
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
    foreach ($Relative in @('release/cojvr.ps1','release/cojvr-gui.ps1','release/release_common.ps1','tools/unstage_d3d9_proxy.ps1','tools/coj_executable_identity.ps1','tools/coj_executable_identity.ps1','tools/deployment_transaction.ps1','tools/coj_video_profile.ps1','tools/openvr_input_manifest.ps1','tools/coj_resolution_settings.ps1')) {
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
    # Run the relocated public Status action, with real persisted metadata but no
    # native helper/device/game. Wrong identity admission or raw error echo must fail.
    $StatusGame=Join-Path $Root 'status-only game'
    New-Item -ItemType Directory -Path $StatusGame | Out-Null
    $StatusStatePath=Join-Path $StatusGame '.cojvr-public-install.json'
    $StatusStagePath=Join-Path $StatusGame '.cojvr-d3d9-stage.json'
    $StatusRunPath=Join-Path $StatusGame '.cojvr-run.json'
    $StatusState=@{schemaVersion=1;manifestType='cojvr-public-install';installId=('a'*32);phase='installed';
        buildManifestId=('b'*64);sourceCommit=('c'*40);version='0.1.0-prealpha.1';runId='status-run';manualReload=$true}
    $StatusStage=@{schemaVersion=2;diagnosticMode='d3d9_native_stereo';runId='status-run';buildManifestId=('b'*64)}
    $StatusRun=@{schemaVersion=1;runId='status-run';buildManifestId=('b'*64);status='staged';validation=@{videoProfile=@{
        resolution='1280x720';nativeResolution='preserve';originalResolution='1280x720';windowed=$false;fsaa=0;
        originalSha256=('d'*64);appliedSha256=('e'*64);resolutionProfile='Auto';experimentalResolution=$false;
        resolutionDecision=@{Profile='Auto';Status='preserve_incompatible';Width=0;Height=0;Experimental=$false;
            Proposal=$null;Limits=@{error='texture_axis_limit_exceeded'}}}}}
    function Reset-StatusFixture {
        Write-CojvrPublicJson $StatusStatePath $StatusState
        Write-CojvrPublicJson $StatusStagePath $StatusStage
        Write-CojvrPublicJson $StatusRunPath $StatusRun
    }
    function Invoke-StatusFixture {
        $Before=@(Get-ChildItem -LiteralPath $StatusGame -File | Sort-Object Name | ForEach-Object {(Get-FileHash -LiteralPath $_.FullName).Hash}) -join ','
        $PreferenceBefore=(Get-FileHash -LiteralPath $Preference).Hash
        $Output=(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Package 'cojvr.ps1') `
            -Action Status -GameDirectory $StatusGame -ResolutionSettingsPath $Preference 2>&1 | Out-String)
        Assert-True ($LASTEXITCODE -eq 0) "Read-only Status failed: $Output"
        $After=@(Get-ChildItem -LiteralPath $StatusGame -File | Sort-Object Name | ForEach-Object {(Get-FileHash -LiteralPath $_.FullName).Hash}) -join ','
        Assert-True ($Before -eq $After -and $PreferenceBefore -eq (Get-FileHash -LiteralPath $Preference).Hash) 'Status changed preparation or preference files'
        return $Output
    }
    Reset-StatusFixture
    $Output=Invoke-StatusFixture
    Assert-True ($Output -match 'configured startup request 1280x720' -and $Output -match 'texture.*dimension.*limit') 'Status omitted the latest preserved preparation outcome'
    Assert-True ($Output -match 'raster.*not observed') 'Status implied native raster execution'
    # A new saved Manual choice is next-session intent, not the prepared size.
    . (Join-Path $SourceDirectory 'tools/coj_resolution_settings.ps1')
    [void](Write-CoJResolutionSettings -Path $Preference -Profile Manual -Width 2804 -Height 2860)
    $Output=Invoke-StatusFixture
    Assert-True ($Output -match 'Next session resolution: Manual 2804x2860' -and
        $Output -match 'Prepared resolution: Auto' -and $Output -match 'Prepare session') 'Changed saved choice was confused with latest preparation'
    $Prepared=$StatusRun.validation.videoProfile
    $Prepared.resolution='2804x2860';$Prepared.experimentalResolution=$true
    $Prepared.resolutionDecision.Status='experimental_trial';$Prepared.resolutionDecision.Experimental=$true
    $Prepared.resolutionDecision.Width=2804;$Prepared.resolutionDecision.Height=2860
    Reset-StatusFixture
    $Output=Invoke-StatusFixture
    Assert-True ($Output -match 'configured startup request 2804x2860' -and $Output -match 'raster.*not observed') 'Status lost successful startup request distinction'
    foreach($Case in @('missing-install','missing-stage','run-mismatch','build-mismatch','stage-mismatch','boolean-run-identity','boolean-stage-identity','boolean-mode','boolean-schema','old-stage-schema','pending','installing','malformed','oversized','invalid-output','invalid-profile','invalid-status','boolean-decision-profile')) {
        Reset-StatusFixture
        switch($Case){
            'missing-install' {Remove-Item -LiteralPath $StatusStagePath;Remove-Item -LiteralPath $StatusStatePath}
            'missing-stage' {Remove-Item -LiteralPath $StatusStagePath}
            'run-mismatch' {$r=Get-Content $StatusRunPath -Raw | ConvertFrom-Json;$r.runId='older-run';Write-CojvrPublicJson $StatusRunPath $r}
            'build-mismatch' {$r=Get-Content $StatusRunPath -Raw | ConvertFrom-Json;$r.buildManifestId=('f'*64);Write-CojvrPublicJson $StatusRunPath $r}
            'stage-mismatch' {$r=Get-Content $StatusStagePath -Raw | ConvertFrom-Json;$r.runId='older-run';Write-CojvrPublicJson $StatusStagePath $r}
            'boolean-run-identity' {$r=Get-Content $StatusRunPath -Raw | ConvertFrom-Json;$r.runId=$true;$r.buildManifestId=$true;Write-CojvrPublicJson $StatusRunPath $r}
            'boolean-stage-identity' {$r=Get-Content $StatusStagePath -Raw | ConvertFrom-Json;$r.runId=$true;$r.buildManifestId=$true;Write-CojvrPublicJson $StatusStagePath $r}
            'boolean-mode' {$r=Get-Content $StatusStagePath -Raw | ConvertFrom-Json;$r.diagnosticMode=$true;Write-CojvrPublicJson $StatusStagePath $r}
            'boolean-schema' {$r=Get-Content $StatusRunPath -Raw | ConvertFrom-Json;$r.schemaVersion=$true;Write-CojvrPublicJson $StatusRunPath $r}
            'old-stage-schema' {$r=Get-Content $StatusStagePath -Raw | ConvertFrom-Json;$r.schemaVersion=1;Write-CojvrPublicJson $StatusStagePath $r}
            'pending' {$r=Get-Content $StatusStatePath -Raw | ConvertFrom-Json;$r | Add-Member pendingRunId 'pending-run';Write-CojvrPublicJson $StatusStatePath $r}
            'installing' {$r=Get-Content $StatusStatePath -Raw | ConvertFrom-Json;$r.phase='installing';Write-CojvrPublicJson $StatusStatePath $r}
            'malformed' {[IO.File]::WriteAllText($StatusRunPath,'{"private-path":"C:/secret-fixture/hidden"')}
            'oversized' {[IO.File]::WriteAllText($StatusRunPath,(' '*65537))}
            'invalid-output' {$r=Get-Content $StatusRunPath -Raw | ConvertFrom-Json;$r.validation.videoProfile.resolution='C:/secret-fixture/hidden';Write-CojvrPublicJson $StatusRunPath $r}
            'invalid-profile' {$r=Get-Content $StatusRunPath -Raw | ConvertFrom-Json;$r.validation.videoProfile.resolutionProfile='C:/secret-fixture/hidden';Write-CojvrPublicJson $StatusRunPath $r}
            'invalid-status' {$r=Get-Content $StatusRunPath -Raw | ConvertFrom-Json;$r.validation.videoProfile.resolutionDecision.Status='C:/secret-fixture/hidden';Write-CojvrPublicJson $StatusRunPath $r}
            'boolean-decision-profile' {$r=Get-Content $StatusRunPath -Raw | ConvertFrom-Json;$r.validation.videoProfile.resolutionDecision.Profile=$true;Write-CojvrPublicJson $StatusRunPath $r}
        }
        $Output=Invoke-StatusFixture
        Assert-True ($Output -match 'preparation outcome: unavailable' -and $Output -notmatch 'configured startup request') "Stale/invalid preparation was reported as current: $Case"
        Assert-True (-not $Output.Contains('C:/secret-fixture/hidden')) 'Status leaked untrusted metadata'
    }
    Reset-StatusFixture
    $Prepared.resolutionDecision.Status='preserve_unavailable';$Prepared.experimentalResolution=$false
    $Prepared.resolutionDecision.Experimental=$false
    $Prepared.resolutionDecision.Limits=@{error='C:/secret-fixture/hidden'}
    Write-CojvrPublicJson $StatusRunPath $StatusRun
    $Output=Invoke-StatusFixture
    Assert-True ($Output -match 'native.*limits.*unavailable' -and -not $Output.Contains('C:/secret-fixture/hidden')) 'Status echoed arbitrary helper error text'
    Write-Host 'PASS read-only resolution Status: current outcome, pending choice, identity fences and safe errors.'
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
