#Requires -Version 5.1
param(
    [ValidateSet('Menu','Install','Uninstall','Diagnose','Status','Verify','PrepareSession')]
    [string]$Action = 'Menu',
    [string]$GameDirectory = '',
    [switch]$ManualReload,
    [string]$VideoSettingsPath = ''
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'release_common.ps1')

function Assert-GameClosed {
    if (Get-Process -Name CoJ -ErrorAction SilentlyContinue) { throw 'Close Call of Juarez normally before this operation.' }
}
function Assert-PublicStageIdentity([string]$Game, [object]$State) {
    $JournalPath = Join-Path $Game '.cojvr-deployment-transaction.json'
    if (Test-Path -LiteralPath $JournalPath) {
        Assert-CojvrNoReparse $JournalPath
        $Journal = [IO.File]::ReadAllText($JournalPath) | ConvertFrom-Json
        $OwnedRunIds = @([string]$State.runId)
        if ($State.PSObject.Properties['pendingRunId']) { $OwnedRunIds += [string]$State.pendingRunId }
        if ($Journal.runId -notin $OwnedRunIds -or $Journal.diagnosticMode -ne 'd3d9_native_stereo' -or
            $Journal.manifestType -ne 'cojvr-deployment-transaction' -or $Journal.operation -notin @('stage','unstage')) {
            throw 'Recovery journal belongs to another deployment. Preserve its backups.'
        }
        $Allowed = @{'d3d9.dll'='d3d9.cojvr-backup.dll';'CoJ.exe'='CoJ.cojvr-backup.exe';
            'code.pak'='code.cojvr-backup.pak';'Data0.pak'='Data0.cojvr-backup.pak';
            'openvr_api.dll'='openvr_api.cojvr-backup.dll';'cojvr_openvr_input'='cojvr_openvr_input.cojvr-backup';
            'cojvr-camera-control.json'='cojvr-camera-control.cojvr-backup.json'}
        $Seen = @{}
        foreach ($Asset in @($Journal.assets)) {
            $Name = [string]$Asset.destination
            if (-not $Allowed.ContainsKey($Name) -or $Seen.ContainsKey($Name) -or $Asset.backup -ne $Allowed[$Name] -or
                ($Asset.PSObject.Properties['temporary'] -and $Asset.temporary -and $Asset.temporary -ne $Allowed[$Name].Replace('cojvr-backup','cojvr-installing'))) {
                throw 'Recovery journal contains an unexpected destination.'
            }
            $Seen[$Name] = $true
            foreach ($Relative in @($Name, [string]$Asset.backup)) { Assert-CojvrNoReparse (Join-Path $Game $Relative) }
        }
        if (-not $Seen.ContainsKey('d3d9.dll')) { throw 'Recovery inventory is incomplete.' }
        # The retained journal proves ownership independently of a partial stage
        # JSON. Unstage validates every retained original before any restoration.
        return
    }
    $StagePath = Join-Path $Game '.cojvr-d3d9-stage.json'
    if (Test-Path -LiteralPath $StagePath) {
        $Stage = Get-Content -LiteralPath $StagePath -Raw | ConvertFrom-Json
        $OwnedRunIds = @([string]$State.runId)
        if ($State.PSObject.Properties['pendingRunId']) { $OwnedRunIds += [string]$State.pendingRunId }
        if ($Stage.diagnosticMode -ne 'd3d9_native_stereo' -or $Stage.buildManifestId -ne $State.buildManifestId -or $Stage.runId -notin $OwnedRunIds) {
            throw 'The current staging belongs to another build. Restore it using its original workflow.'
        }
    }
}
function Restore-PublicInstall([string]$Game, [object]$State) {
    Assert-PublicStageIdentity $Game $State
    $Stage = Join-Path $Game '.cojvr-d3d9-stage.json'
    $Journal = Join-Path $Game '.cojvr-deployment-transaction.json'
    if ((Test-Path -LiteralPath $Stage) -or (Test-Path -LiteralPath $Journal)) {
        & (Join-Path $PSScriptRoot 'tools/unstage_d3d9_proxy.ps1') -GameDirectory $Game
    }
    [void](Restore-CoJVrVideoProfile -StatePath (Join-Path $Game '.cojvr-public-video.json') -BackupPath (Join-Path $Game '.cojvr-public-video.backup'))
    Remove-Item -LiteralPath (Join-Path $Game '.cojvr-public-install.json') -Force
}
function Install-PublicRuntime([object]$Manifest) {
    $Required = [version]([string]$Manifest.dependencies.vcRuntimeVersion).TrimStart('v')
    $Runtime = Get-ItemProperty 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x86' -ErrorAction SilentlyContinue
    if ($null -ne $Runtime -and $Runtime.Installed -eq 1 -and
        [version]([string]$Runtime.Version).TrimStart('v') -ge $Required) { return }
    $Installer = Join-Path $PSScriptRoot 'dependencies/vc_redist.x86.exe'
    $Signature = Get-AuthenticodeSignature -LiteralPath $Installer
    if ($Signature.Status -ne 'Valid' -or $Signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
        throw 'The bundled VC++ runtime is not signed by Microsoft. Download the release again.'
    }
    Write-Host 'Installing the included Microsoft Visual C++ x86 runtime. Windows may request administrator approval.'
    $Process = Start-Process -FilePath $Installer -ArgumentList '/install','/passive','/norestart' -Verb RunAs -WindowStyle Hidden -Wait -PassThru
    if ($Process.ExitCode -notin @(0,1638,3010)) { throw "VC++ runtime installation failed (code $($Process.ExitCode))." }
    if ($Process.ExitCode -eq 3010) { throw 'The VC++ runtime requires a Windows restart. Restart, then run Install again; the game has not been patched.' }
}
function Start-PublicSession([string]$Game, [object]$State) {
    Assert-PublicStageIdentity $Game $State
    if ($State.PSObject.Properties['pendingRunId']) {
        throw 'A previous session preparation was interrupted. Use Install/update or Uninstall/recover before preparing another session.'
    }
    $StagePath = Join-Path $Game '.cojvr-d3d9-stage.json'
    $RunPath = Join-Path $Game '.cojvr-run.json'
    $StageText = [IO.File]::ReadAllText($StagePath)
    $RunText = [IO.File]::ReadAllText($RunPath)
    $Stage = $StageText | ConvertFrom-Json
    $Run = $RunText | ConvertFrom-Json
    if ($Run.runId -notmatch '^[A-Za-z0-9._-]+$' -or $Run.runId -in @('.','..') -or
        $Run.runId -ne $Stage.runId -or $Run.buildManifestId -ne $Stage.buildManifestId) {
        throw 'Interrupted session metadata. Use Install/update to restore and prepare a fresh installation.'
    }
    $OldDirectory = Join-Path $Game ('.cojvr-evidence/runs/' + $Run.runId)
    $BuildPath = Join-Path $OldDirectory 'build-manifest.json'
    if ((Get-FileHash -LiteralPath $BuildPath -Algorithm SHA256).Hash -ne $Stage.buildManifestSha256) { throw 'Installed build evidence changed.' }
    $RunId = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ') + '-' + [guid]::NewGuid().ToString('N').Substring(0,12)
    $NewDirectory = Join-Path $Game ('.cojvr-evidence/runs/' + $RunId)
    New-Item -ItemType Directory -Path $NewDirectory | Out-Null
    Copy-Item -LiteralPath $BuildPath -Destination (Join-Path $NewDirectory 'build-manifest.json')
    $StatePath = Join-Path $Game '.cojvr-public-install.json'
    $StateText = [IO.File]::ReadAllText($StatePath)
    $Moved = @()
    $Stage.runId = $RunId
    $Run.runId = $RunId
    $Run.createdUtc = [DateTime]::UtcNow.ToString('o')
    $Run.status = 'staged'
    try {
        # Retain both identities across an interrupted metadata publication.
        $State | Add-Member -NotePropertyName pendingRunId -NotePropertyValue $RunId -Force
        Write-CojvrPublicJson $StatePath $State
        foreach ($Name in @('cojvr.log','callstack.txt','cojvr-native-stereo-summary.json')) {
            $Path = Join-Path $Game $Name
            if (Test-Path -LiteralPath $Path -PathType Leaf) {
                $Archive = Join-Path $OldDirectory ('previous-' + $Name)
                Move-Item -LiteralPath $Path -Destination $Archive -Force
                $Moved += @{source=$Path;archive=$Archive}
            }
        }
        Copy-Item -LiteralPath $RunPath -Destination (Join-Path $OldDirectory 'previous-run.json') -Force
        Write-CojvrPublicJson $StagePath $Stage
        Write-CojvrPublicJson $RunPath $Run
        $State.runId = $RunId
        $State.PSObject.Properties.Remove('pendingRunId')
        Write-CojvrPublicJson $StatePath $State
    } catch {
        [IO.File]::WriteAllText($StagePath, $StageText, [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($RunPath, $RunText, [Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText($StatePath, $StateText, [Text.UTF8Encoding]::new($false))
        foreach ($Entry in $Moved) { Move-Item -LiteralPath $Entry.archive -Destination $Entry.source -Force }
        throw
    }
    Write-Host "Fresh community session: $RunId. Start SteamVR and the game manually."
}
function Invoke-PublicAction([string]$RequestedAction, [string]$Game, [bool]$Reload, [bool]$ReloadSpecified) {
    $Manifest = Read-CojvrRelease $PSScriptRoot
    if ($RequestedAction -eq 'Verify') { Write-Host "Package $($Manifest.version): all files verified."; return }
    if ($RequestedAction -eq 'Diagnose') {
        $Reports = Join-Path $env:LOCALAPPDATA 'CoJVR/Reports'
        $Zip = New-CojvrPublicReport -GameDirectory $Game -OutputDirectory $Reports -Version $Manifest.version
        Write-Host "Report saved locally: $Zip"
        Write-Host 'Inspect it for private information, then attach manually to a GitHub issue with your chapter, visor/controllers and reproduction steps.'
        return
    }
    $State = Assert-CojvrPublicOwnership $Game
    if ($RequestedAction -eq 'Status') {
        Write-Host "Package: $($Manifest.version)"
        Write-Host "Game: $Game"
        if ($null -eq $State) { Write-Host 'Public mod is not installed.' } else {
            Write-Host "Installed: $($State.version); phase: $($State.phase); manual reload: $($State.manualReload)"
        }
        return
    }
    Assert-GameClosed
    if (-not [Environment]::Is64BitOperatingSystem) { throw 'This package requires 64-bit Windows; the game remains x86.' }
    # Check originals before creating anything in the game directory.
    if ($RequestedAction -eq 'Install' -and $null -eq $State) { Assert-CojvrPublicGame $Game }
    $LockPath = Join-Path $Game '.cojvr-public.lock'
    Assert-CojvrNoReparse $LockPath
    $Lock = [IO.File]::Open($LockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try {
        . (Join-Path $PSScriptRoot 'tools/coj_video_profile.ps1')
        if ($RequestedAction -eq 'Uninstall') {
            if ($null -eq $State) { Write-Host 'Public mod is not installed.'; return }
            Restore-PublicInstall $Game $State
            Write-Host 'Original game files and video settings restored. VR preferences and local reports retained.'
            return
        }
        if ($RequestedAction -eq 'PrepareSession') {
            if ($null -eq $State -or $State.phase -ne 'installed') { throw 'Install the mod or recover its interrupted installation first.' }
            Start-PublicSession $Game $State
            return
        }
        if ($null -ne $State -and -not $ReloadSpecified) { $Reload = [bool]$State.manualReload }
        if ($null -ne $State) { Assert-PublicStageIdentity $Game $State }
        if ($null -ne $State -and $State.phase -eq 'installed') { Assert-CojvrPublicGame $Game -Installed }
        $VideoPath = $VideoSettingsPath
        if ([string]::IsNullOrWhiteSpace($VideoPath)) {
            $VideoPath = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'call of juarez/out/Settings/Video.scr'
        }
        if (-not (Test-Path -LiteralPath $VideoPath -PathType Leaf)) { throw 'Run the original game in D3D9 mode once to create Video.scr before installation.' }
        Assert-CojvrNoReparse $VideoPath
        $Python = Join-Path $PSScriptRoot 'dependencies/python/python.exe'
        & $Python -B -c 'import zipfile, pathlib, inspect_java_bytecode, coj_manual_reload, coj_physical_punch'
        if ($LASTEXITCODE -ne 0) { throw 'Bundled Python/patcher self-check failed. Extract the complete release again.' }
        Install-PublicRuntime $Manifest
        if ($null -ne $State) { Restore-PublicInstall $Game $State }
        Assert-CojvrPublicGame $Game
        if (Test-Path -LiteralPath (Join-Path $Game 'd3d9.dll')) { throw 'Another d3d9.dll mod is installed. Remove it with its own installer first.' }
        $Build = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'payload/d3d9_native_stereo.build-manifest.json') -Raw | ConvertFrom-Json
        $InstallId = [guid]::NewGuid().ToString('N')
        $State = [ordered]@{ schemaVersion=1;manifestType='cojvr-public-install';installId=$InstallId;runId=('public-' + $InstallId);version=$Manifest.version;sourceCommit=$Manifest.sourceCommit;buildManifestId=$Build.manifestId;phase='installing';manualReload=$Reload }
        Write-CojvrPublicJson (Join-Path $Game '.cojvr-public-install.json') $State
        try {
            & (Join-Path $PSScriptRoot 'tools/stage_d3d9_proxy.ps1') -GameDirectory $Game `
                -ProxyPath (Join-Path $PSScriptRoot 'payload/d3d9_native_stereo.dll') `
                -BuildManifestPath (Join-Path $PSScriptRoot 'payload/d3d9_native_stereo.build-manifest.json') `
                -RunId $State.runId -PythonExecutable $Python -IndependentHands -LargeAddressAware -ManualReload:$Reload
            & (Join-Path $PSScriptRoot 'tools/set_hmd_camera_control.ps1') -GameDirectory $Game -Mode enable
            & (Join-Path $PSScriptRoot 'tools/set_hmd_camera_control.ps1') -GameDirectory $Game -Mode body-enable
            [void](Apply-CoJVrVideoProfile -VideoPath $VideoPath -StatePath (Join-Path $Game '.cojvr-public-video.json') -BackupPath (Join-Path $Game '.cojvr-public-video.backup'))
            $State.phase = 'installed'
            Write-CojvrPublicJson (Join-Path $Game '.cojvr-public-install.json') $State
        } catch {
            $Failure = $_.Exception.Message
            try { Restore-PublicInstall $Game ([pscustomobject]$State) } catch {
                throw "Installation failed: $Failure Restoration requires attention: $($_.Exception.Message). Keep all backups and rerun Uninstall."
            }
            throw "Installation failed; original game restored: $Failure"
        }
        Write-Host "Installed $($Manifest.version). Start SteamVR, then launch Call of Juarez in D3D9 mode manually."
        Write-Host 'Use Prepare session before each subsequent play session for a fresh report identity. Campaign completion and other controllers remain experimental.'
    } finally { $Lock.Dispose() }
}

try {
    if ($Action -ne 'Menu') {
        $Game = if ($Action -eq 'Verify') { '' } else { Resolve-CojvrPublicGame $GameDirectory }
        Invoke-PublicAction $Action $Game $ManualReload.IsPresent $PSBoundParameters.ContainsKey('ManualReload')
    } else {
        Write-Host 'Call of Juarez VR - community pre-alpha'
        Write-Host 'Experimental campaign support. Back up your saves before testing.'
        try { $Game = Resolve-CojvrPublicGame $GameDirectory } catch {
            $Game = Resolve-CojvrPublicGame (Read-Host 'Game folder containing CoJ.exe')
        }
        while ($true) {
            Write-Host "`nGame: $Game"
            Write-Host '1 Install/update | 2 Uninstall/recover | 3 Diagnostics | 4 Status | 5 Verify package | 6 Prepare session | 0 Exit'
            $Choice = Read-Host 'Choose'
            if ($Choice -eq '0') { break }
            $Requested = @{'1'='Install';'2'='Uninstall';'3'='Diagnose';'4'='Status';'5'='Verify';'6'='PrepareSession'}[$Choice]
            if ($null -eq $Requested) { continue }
            $Reload = $false
            $Specified = $false
            if ($Requested -eq 'Install') {
                $Answer = Read-Host 'Experimental manual reload? y/n (Enter preserves installed choice; default n)'
                $Specified = -not [string]::IsNullOrWhiteSpace($Answer)
                $Reload = $Answer -match '^(y|yes|s|si)$'
            }
            try { Invoke-PublicAction $Requested $Game $Reload $Specified } catch { Write-Host ("ERROR: " + $_.Exception.Message) -ForegroundColor Red }
        }
    }
} catch { Write-Error $_; exit 1 }
