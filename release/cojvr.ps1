#Requires -Version 5.1
param(
    [ValidateSet('Menu','Install','Uninstall','Diagnose','Status','Verify','PrepareSession','ConfigureResolution')]
    [string]$Action = 'Menu',
    [string]$GameDirectory = '',
    [switch]$ManualReload = $true,
    [string]$VideoSettingsPath = '',
    [ValidateSet('Preserve','Auto','75%','100%','125%','Manual')]
    [string]$ResolutionProfile = 'Auto',
    [object]$ResolutionWidth = 0,
    [object]$ResolutionHeight = 0,
    [string]$ResolutionSettingsPath = ''
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
    Assert-PublicVideoRecoveryPaths $Game
    [void](Restore-CoJVrVideoProfile -StatePath (Join-Path $Game '.cojvr-session-video.json') -BackupPath (Join-Path $Game '.cojvr-session-video.backup'))
    $Stage = Join-Path $Game '.cojvr-d3d9-stage.json'
    $Journal = Join-Path $Game '.cojvr-deployment-transaction.json'
    if ((Test-Path -LiteralPath $Stage) -or (Test-Path -LiteralPath $Journal)) {
        & (Join-Path $PSScriptRoot 'tools/unstage_d3d9_proxy.ps1') -GameDirectory $Game
    }
    [void](Restore-CoJVrVideoProfile -StatePath (Join-Path $Game '.cojvr-public-video.json') -BackupPath (Join-Path $Game '.cojvr-public-video.backup'))
    Remove-Item -LiteralPath (Join-Path $Game '.cojvr-public-install.json') -Force
}
function Assert-PublicVideoRecoveryPaths([string]$Game) {
    foreach ($Layer in @('public','session')) {
        $Journal = Join-Path $Game ('.cojvr-' + $Layer + '-video.json')
        Assert-CojvrNoReparse $Journal
        Assert-CojvrNoReparse (Join-Path $Game ('.cojvr-' + $Layer + '-video.backup'))
        if (Test-Path -LiteralPath $Journal -PathType Leaf) {
            $VideoState = Get-Content -LiteralPath $Journal -Encoding UTF8 -Raw | ConvertFrom-Json
            Assert-CojvrNoReparse ([string]$VideoState.videoPath) -AllowCloudFiles
        }
    }
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
function Read-PublicResolutionSettings {
    $Path=$ResolutionSettingsPath
    if ([string]::IsNullOrWhiteSpace($Path)) {$Path=Get-CoJResolutionSettingsPath}
    return Read-CoJResolutionSettings -Path $Path
}
function Resolve-PublicResolution($Settings) {
    return Resolve-CoJLaunchResolution -Settings $Settings `
        -PythonExecutable (Join-Path $PSScriptRoot 'dependencies/python/python.exe') `
        -ResolverPath (Join-Path $PSScriptRoot 'tools/coj_resolution_profiles.py') `
        -QueryHelperPath (Join-Path $PSScriptRoot 'tools/cojvr_openvr_resolution_query.exe') `
        -LimitsHelperPath (Join-Path $PSScriptRoot 'tools/cojvr_d3d9_resolution_limits.exe')
}
function Assert-PublicNativeResolutionContext([string]$Game) {
    $Stage=Get-Content -LiteralPath (Join-Path $Game '.cojvr-d3d9-stage.json') -Raw | ConvertFrom-Json
    $Expected='C8B8BB82FCB3D6599C5F77B1BB9CB3444CBB3A360461DAD43AD808B49AD28DC9'
    if ($Stage.stagedExecutableSha256 -ne $Expected -or
        (Get-FileHash -LiteralPath (Join-Path $Game 'CoJ.exe') -Algorithm SHA256).Hash -ne $Expected -or
        ($Stage.PSObject.Properties['independentEyeResolution'] -and $Stage.independentEyeResolution -ne 'off')) {
        throw 'Resolution profiles require the recognized LAA executable and ordinary native stereo. Recover/install first.'
    }
}
function Get-PublicResolutionFallbackReason([object]$Decision) {
    # Helper errors can contain local paths/exception text. Only known symbols
    # select fixed, bounded messages; no helper or persisted error is echoed.
    try {
        if ($Decision.Status -eq 'preserve_adapter_unqualified') {
            return 'The selected graphics adapter is not qualified for startup resolution.'
        }
        if ($null -ne $Decision.Limits) {
            switch -CaseSensitive ($Decision.Limits.error) {
                'texture_axis_limit_exceeded' {return 'Requested size exceeds the native texture dimension limit.'}
                'texture_aspect_ratio_limit_exceeded' {return 'Requested aspect ratio exceeds the native texture limit.'}
                'square_texture_required' {return 'The native adapter requires square textures.'}
                'power_of_two_texture_required' {return 'The native adapter requires power-of-two texture dimensions.'}
                'required_format_unsupported' {return 'A required native color or depth format is unsupported.'}
                'implementation_axis_limit_exceeded' {return 'Requested size exceeds the startup dimension limit.'}
                'menu_source_128mib_limit_exceeded' {return 'Requested size exceeds the menu source size limit.'}
                'nominal_payload_reaches_x86_laa_ceiling' {return 'Requested size exceeds the startup address-space limit.'}
            }
            if ($Decision.Status -eq 'preserve_incompatible') {return 'Requested size is incompatible with native resolution limits.'}
            return 'Native resolution limits are unavailable.'
        }
        $Proposal=$Decision.Proposal
        if ($null -ne $Proposal -and $Proposal.PSObject.Properties['observation'] -and $null -ne $Proposal.observation) {
            switch -CaseSensitive ($Proposal.observation.error) {
                'runtime_not_installed' {return 'SteamVR is not installed.'}
                'hmd_not_present' {return 'The headset was not detected by SteamVR.'}
                'background_init_failed' {return 'SteamVR is unavailable for the headset query.'}
                'system_unavailable' {return 'The SteamVR system interface is unavailable.'}
                'invalid_recommended_dimensions' {return 'SteamVR returned an invalid recommended size.'}
            }
        }
        if ($null -ne $Proposal) {
            switch -Regex -CaseSensitive ($Proposal.reason) {
                '\Aquery_helper_unavailable(?::|\z)' {return 'The headset query helper is unavailable.'}
                '\Aquery_helper_failed\z' {return 'The headset query helper failed.'}
                '\Aresolver_unavailable(?::|\z)' {return 'The startup resolution calculator is unavailable.'}
                '\Aresolver_failed\z' {return 'The startup resolution calculator failed.'}
                '\Afresh_recommendation_required\z' {return 'A fresh SteamVR recommendation is required.'}
            }
        }
    } catch { } # Older or incomplete metadata receives the same safe fallback.
    return 'The requested startup resolution is unavailable.'
}
function Show-PublicResolutionOutcome([object]$VideoProfile) {
    try {
        $Profile=$VideoProfile.resolutionProfile
        $Decision=$VideoProfile.resolutionDecision
        $Size=$VideoProfile.resolution
        $Width=0;$Height=0
        if ($Profile -isnot [string] -or @('Preserve','Auto','75%','100%','125%','Manual') -cnotcontains $Profile -or
            $Size -isnot [string] -or $Size -cnotmatch '\A([1-9][0-9]{0,9})x([1-9][0-9]{0,9})\z' -or
            -not [int]::TryParse($Matches[1],[ref]$Width) -or -not [int]::TryParse($Matches[2],[ref]$Height) -or
            $Decision.Profile -isnot [string] -or $Decision.Profile -cne $Profile -or $Decision.Experimental -isnot [bool] -or
            $VideoProfile.experimentalResolution -isnot [bool] -or $VideoProfile.experimentalResolution -ne $Decision.Experimental -or
            $Decision.Status -isnot [string] -or
            @('preserve','experimental_trial','preserve_unavailable','preserve_incompatible','preserve_adapter_unqualified') -cnotcontains $Decision.Status -or
            (($Decision.Status -ceq 'experimental_trial') -ne $Decision.Experimental) -or
            (($Decision.Status -ceq 'preserve') -ne ($Profile -ceq 'Preserve'))) {throw 'Invalid resolution outcome.'}
    } catch {
        Write-Host 'Latest preparation outcome: unavailable (invalid or missing resolution metadata).'
        return
    }
    Write-Host "Prepared resolution: $Profile; configured startup request ${Width}x${Height}."
    if ($Decision.Experimental) {
        Write-Host 'Outcome: startup request written to game video settings.'
    } elseif ($Profile -eq 'Preserve') {
        Write-Host 'Outcome: selected game size retained (Preserve).'
    } else {
        Write-Host ('Outcome: selected game size retained. '+(Get-PublicResolutionFallbackReason $Decision))
        Write-Host 'Retry with 6 - Prepare session while the game is closed; start SteamVR first for Auto/percentages. Use 7 - Resolution profiles to change the request.'
    }
    Write-Host 'Actual native raster: not observed by the launcher.'
}
function Show-PublicResolutionStatus([string]$Game,[object]$State) {
    try {
        if ($null -eq $State -or $State.phase -cne 'installed' -or $State.PSObject.Properties['pendingRunId']) {
            throw 'No completed installation preparation.'
        }
        $Records=@{}
        foreach ($Name in @('.cojvr-d3d9-stage.json','.cojvr-run.json')) {
            $Path=Join-Path $Game $Name
            Assert-CojvrNoReparse $Path
            # Hold the read handle across the bound check and parse. Status never
            # queries OpenVR or repairs incomplete/foreign preparation metadata.
            $Stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
            try {
                if ($Stream.Length -gt 65536) {throw 'Preparation metadata exceeds the reporting bound.'}
                $Reader=[IO.StreamReader]::new($Stream,[Text.Encoding]::UTF8,$true)
                try {$Records[$Name]=$Reader.ReadToEnd() | ConvertFrom-Json -ErrorAction Stop}
                finally {$Reader.Dispose()}
            } finally {$Stream.Dispose()}
        }
        $Stage=$Records['.cojvr-d3d9-stage.json'];$Run=$Records['.cojvr-run.json']
        foreach ($Record in @($State,$Stage,$Run)) {
            if ($Record.runId -isnot [string] -or $Record.buildManifestId -isnot [string]) {
                throw 'Invalid preparation identity types.'
            }
        }
        if (($Stage.schemaVersion -isnot [int] -and $Stage.schemaVersion -isnot [long]) -or $Stage.schemaVersion -ne 2 -or
            ($Run.schemaVersion -isnot [int] -and $Run.schemaVersion -isnot [long]) -or $Run.schemaVersion -ne 1 -or
            $Stage.diagnosticMode -isnot [string] -or $Stage.diagnosticMode -cne 'd3d9_native_stereo' -or
            $Stage.runId -cne $State.runId -or $Run.runId -cne $State.runId -or
            $Stage.buildManifestId -cne $State.buildManifestId -or $Run.buildManifestId -cne $State.buildManifestId) {
            throw 'Preparation identities do not match the installed session.'
        }
        $VideoProfile=$Run.validation.videoProfile
    } catch {
        Write-Host 'Latest preparation outcome: unavailable (missing, interrupted or mismatched preparation metadata).'
        return
    }
    Write-Host 'Latest preparation outcome:'
    Show-PublicResolutionOutcome $VideoProfile
}
function Update-PublicSessionVideo([string]$Game,[object]$Run) {
    Assert-PublicVideoRecoveryPaths $Game
    $Baseline=Get-Content -LiteralPath (Join-Path $Game '.cojvr-public-video.json') -Encoding UTF8 -Raw | ConvertFrom-Json
    $Video=[string]$Baseline.videoPath
    if ([string]::IsNullOrWhiteSpace($Video) -or $Baseline.appliedSha256 -notmatch '^[A-Fa-f0-9]{64}$') {
        throw 'Missing or invalid installation video baseline; recover/install first.'
    }
    $Decision=Resolve-PublicResolution (Read-PublicResolutionSettings)
    if ($Decision.Experimental) {Assert-PublicNativeResolutionContext $Game}
    $Journal=Join-Path $Game '.cojvr-session-video.json'
    $Backup=Join-Path $Game '.cojvr-session-video.backup'
    if (Test-Path -LiteralPath $Journal -PathType Leaf) {
        $Previous=Get-Content -LiteralPath $Journal -Encoding UTF8 -Raw | ConvertFrom-Json
        if ($Previous.videoPath -ne $Video -or $Previous.originalSha256 -ne $Baseline.appliedSha256) {
            throw 'Session video baseline does not match the installation. Keep backups and recover first.'
        }
        [void](Restore-CoJVrVideoProfile -StatePath $Journal -BackupPath $Backup)
    }
    if ((Get-FileHash -LiteralPath $Video -Algorithm SHA256).Hash -ne $Baseline.appliedSha256) {
        throw 'Installation video baseline changed outside a session. Recover/install before applying profiles.'
    }
    $Applied=Apply-CoJVrVideoProfile -VideoPath $Video -StatePath $Journal -BackupPath $Backup `
        -ExperimentalWidth $Decision.Width -ExperimentalHeight $Decision.Height
    $Run.validation | Add-Member -NotePropertyName videoProfile -NotePropertyValue ([ordered]@{
        resolution=$Applied.resolution;nativeResolution=$Applied.nativeResolution;
        originalResolution=$Applied.originalResolution;windowed=$Applied.windowed;fsaa=$Applied.fsaa;
        originalSha256=$Applied.originalSha256;appliedSha256=$Applied.appliedSha256;
        resolutionProfile=$Decision.Profile;experimentalResolution=$Decision.Experimental;resolutionDecision=$Decision
    }) -Force
    Show-PublicResolutionOutcome $Run.validation.videoProfile
}
function Show-PublicResolutionSelector {
    $Current=Read-PublicResolutionSettings
    $Extent=if($Current.profile -eq 'Manual'){" $($Current.width)x$($Current.height)"}else{''}
    Write-Host "`nResolution profile: $($Current.profile)$Extent"
    Write-Host '1 Preserve game settings | 2 Auto | 3 75% | 4 100% | 5 125% | 6 Manual | 0 Cancel'
    Write-Host 'Explicit profiles are experimental startup choices. Percentages scale pixel count relative to the SteamVR query.'
    $Choice=Read-Host 'Profile'
    if ($Choice -eq '0') {return}
    $Profile=@{'1'='Preserve';'2'='Auto';'3'='75%';'4'='100%';'5'='125%';'6'='Manual'}[$Choice]
    if (-not $Profile) {throw 'Invalid profile choice.'}
    $Width=0;$Height=0
    if ($Profile -eq 'Manual') {$Width=Read-Host 'Width per eye';$Height=Read-Host 'Height per eye'}
    $Path=$ResolutionSettingsPath
    if ([string]::IsNullOrWhiteSpace($Path)) {$Path=Get-CoJResolutionSettingsPath}
    [void](Write-CoJResolutionSettings -Path $Path -Profile $Profile -Width $Width -Height $Height)
    Write-Host 'Saved. Choose 6 - Prepare session with the game closed to apply. Start SteamVR first for Auto/percentage profiles.'
}
function Show-PublicManualReloadSelector([string]$Game) {
    [void](Read-CojvrRelease $PSScriptRoot)
    Assert-GameClosed
    $State = Assert-CojvrPublicOwnership $Game
    $Current = if ($null -eq $State) { 'On (new installation default)' } elseif ($State.manualReload) { 'On' } else { 'Off' }
    Write-Host "`nManual reload: $Current"
    Write-Host '1 On - motion-controller reload | 2 Off - conventional reload | 0 Cancel'
    Write-Host 'Applies through Install/update with the game closed. Tracked head/body, independent hands and physical melee remain enabled.'
    $Choice = Read-Host 'Manual reload'
    if ($Choice -eq '0') { return }
    if ($Choice -notin @('1','2')) { throw 'Invalid manual reload choice.' }
    Invoke-PublicAction 'Install' $Game ($Choice -eq '1') $true
}
function Show-PublicLauncherHelp {
    Write-Host "`nStartup: start SteamVR manually -> Install/update (first play or update), or Prepare session (later plays) -> launch the game manually in D3D9 mode."
    Write-Host 'Keep the game closed during Install/update, Prepare session and reload changes. SteamVR must be running first for Auto/percentage resolution queries.'
    Write-Host "`nDefault profile      Pause / hold for VR settings       Accept / Back in menus"
    Write-Host 'PS VR2 Sense         R Options                          R Cross / Circle'
    Write-Host 'Oculus/Meta Touch    R stick UP + click                  R A / B'
    Write-Host 'Valve Index          R thumbstick UP + click             R A / B'
    Write-Host 'HTC Vive wands       R application-menu button           R pad UP / DOWN click'
    Write-Host 'WMR                  R application-menu button           R pad UP / DOWN click'
    Write-Host 'Press Pause once for the native menu. VR settings are in Options > Game; holding Pause does not open a quick panel. UP + click means keep the stick upward while pressing it; center sticks and release controls before resuming.'
    Write-Host 'Save/load: open the native pause menu, then select Save/Load when the campaign permits. Either hand trigger selects with its menu pointer; profile Accept/Back controls also work.'
    Write-Host 'Quick Save/Quick Load have no default controller binding. Optional actions can be assigned in SteamVR bindings. Custom bindings may differ from this table.'
    Write-Host 'Additional controller profiles are experimental. In-game prompts currently use Sense labels.'
    Write-Host 'Native Options > Game: cinematic presentation, smooth/snap turning and weapon guides (default off).'
    Write-Host 'Sense bow: left hand aims; right hand at string -> fresh R2 hold -> draw back -> release R2 to shoot.'
    Write-Host 'Sense rifle support: fresh L1 hold near the wooden fore-end; release to detach. Square keeps automatic native reload. Remington grasp has scoped acceptance; Winchester/SawOff remain experimental.'
    Write-Host 'Sense holsters: fresh R1 at right hip/right shoulder, L1 at left hip/left shoulder; release between gestures. Shoulder zones select/stow long gun/bow. Native inventory permissions apply.'
    Write-Host 'Duels: center right stick; right hand at hip + stick down to draw when permitted, then physically raise hand + stick up. Native timing still applies.'
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
    $Moved = @()
    $Stage.runId = $RunId
    $Run.runId = $RunId
    $Run.createdUtc = [DateTime]::UtcNow.ToString('o')
    $Run.status = 'staged'
    try {
        # Retain both identities across an interrupted metadata publication.
        $State | Add-Member -NotePropertyName pendingRunId -NotePropertyValue $RunId -Force
        Write-CojvrPublicJson $StatePath $State
        Update-PublicSessionVideo $Game $Run
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
        # Keep pendingRunId durable: a failed video/metadata transaction must
        # recover its session layer before another preparation can be admitted.
        foreach ($Entry in $Moved) { Move-Item -LiteralPath $Entry.archive -Destination $Entry.source -Force }
        throw "Session preparation failed. Keep backups and use Uninstall/recover or Install/update: $($_.Exception.Message)"
    }
    Write-Host "Fresh community session: $RunId. Keep SteamVR running and launch the game manually in D3D9 mode."
}
function Invoke-PublicAction([string]$RequestedAction, [string]$Game, [bool]$Reload, [bool]$ReloadSpecified) {
    $Manifest = Read-CojvrRelease $PSScriptRoot
    if ($RequestedAction -eq 'Verify') { Write-Host "Package $($Manifest.version): all files verified."; return }
    if ($RequestedAction -eq 'ConfigureResolution') {
        Assert-GameClosed
        . (Join-Path $PSScriptRoot 'tools/coj_resolution_settings.ps1')
        $Path=$ResolutionSettingsPath
        if ([string]::IsNullOrWhiteSpace($Path)) {$Path=Get-CoJResolutionSettingsPath}
        [void](Write-CoJResolutionSettings -Path $Path -Profile $ResolutionProfile -Width $ResolutionWidth -Height $ResolutionHeight)
        Write-Host "Saved resolution profile: $ResolutionProfile. Apply through Prepare session while the game is closed."
        return
    }
    if ($RequestedAction -eq 'Diagnose') {
        $Reports = Join-Path $env:LOCALAPPDATA 'CoJVR/Reports'
        $Zip = New-CojvrPublicReport -GameDirectory $Game -OutputDirectory $Reports -Version $Manifest.version
        Write-Host "Report saved locally: $Zip"
        Write-Host 'Inspect it for private information, then attach manually to a GitHub issue with your chapter, visor/controllers and reproduction steps.'
        return
    }
    $State = Assert-CojvrPublicOwnership $Game
    if ($RequestedAction -eq 'Status') {
        . (Join-Path $PSScriptRoot 'tools/coj_resolution_settings.ps1')
        $Selected=Read-PublicResolutionSettings
        $Extent=if($Selected.profile -eq 'Manual'){" $($Selected.width)x$($Selected.height)"}else{''}
        Write-Host "Next session resolution: $($Selected.profile)$Extent"
        Write-Host 'Changes to this saved choice require Install/update or 6 - Prepare session with the game closed.'
        Show-PublicResolutionStatus $Game $State
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
        . (Join-Path $PSScriptRoot 'tools/coj_resolution_settings.ps1')
        . (Join-Path $PSScriptRoot 'tools/coj_resolution_session.ps1')
        [void](Read-PublicResolutionSettings)
        if ($RequestedAction -eq 'PrepareSession') {
            if ($null -eq $State -or $State.phase -ne 'installed') { throw 'Install the mod or recover its interrupted installation first.' }
            Assert-CojvrPublicGame $Game -Installed
            Start-PublicSession $Game $State
            return
        }
        if (-not $ReloadSpecified) {
            $Reload = if ($null -ne $State) { [bool]$State.manualReload } else { $true }
        }
        if ($null -ne $State) { Assert-PublicStageIdentity $Game $State }
        if ($null -ne $State -and $State.phase -eq 'installed') { Assert-CojvrPublicGame $Game -Installed }
        $VideoPath = $VideoSettingsPath
        if ([string]::IsNullOrWhiteSpace($VideoPath)) {
            $VideoPath = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'call of juarez/out/Settings/Video.scr'
        }
        if (-not (Test-Path -LiteralPath $VideoPath -PathType Leaf)) { throw 'Run the original game in D3D9 mode once to create Video.scr before installation.' }
        Assert-CojvrNoReparse $VideoPath -AllowCloudFiles
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
            $RunPath=Join-Path $Game '.cojvr-run.json'
            $Run=Get-Content -LiteralPath $RunPath -Raw | ConvertFrom-Json
            Update-PublicSessionVideo $Game $Run
            Write-CojvrPublicJson $RunPath $Run
            $State.phase = 'installed'
            Write-CojvrPublicJson (Join-Path $Game '.cojvr-public-install.json') $State
        } catch {
            $Failure = $_.Exception.Message
            try { Restore-PublicInstall $Game ([pscustomobject]$State) } catch {
                throw "Installation failed: $Failure Restoration requires attention: $($_.Exception.Message). Keep all backups and rerun Uninstall."
            }
            throw "Installation failed; original game restored: $Failure"
        }
        Write-Host "Installed $($Manifest.version). Keep SteamVR running and launch Call of Juarez in D3D9 mode manually."
        Write-Host "VR gameplay: tracked head/body, independent hands and physical melee. Manual reload: $Reload."
        Write-Host 'Use Prepare session before each subsequent play session for a fresh report identity. Campaign completion and other controllers remain experimental.'
    } finally { $Lock.Dispose() }
}

try {
    if ($Action -ne 'Menu') {
        $Game = if ($Action -in @('Verify','ConfigureResolution')) { '' } else { Resolve-CojvrPublicGame $GameDirectory }
        Invoke-PublicAction $Action $Game $ManualReload.IsPresent $PSBoundParameters.ContainsKey('ManualReload')
    } else {
        Write-Host 'Call of Juarez VR - community pre-alpha'
        Write-Host 'Experimental campaign support. Back up your saves before testing.'
        Write-Host 'Start SteamVR manually -> Install/update or Prepare session with the game closed -> launch the game manually in D3D9 mode.'
        try { $Game = Resolve-CojvrPublicGame $GameDirectory } catch {
            $Game = Resolve-CojvrPublicGame (Read-Host 'Game folder containing CoJ.exe')
        }
        while ($true) {
            Write-Host "`nGame: $Game"
            Write-Host '1 Install/update | 2 Uninstall/recover | 3 Diagnostics | 4 Status | 5 Verify package | 6 Prepare session | 7 Resolution profiles | 8 Manual reload on/off | 9 Controls and startup help | 0 Exit'
            $Choice = Read-Host 'Choose'
            if ($Choice -eq '0') { break }
            if ($Choice -eq '9') { Show-PublicLauncherHelp; continue }
            if ($Choice -eq '8') {
                try { Show-PublicManualReloadSelector $Game } catch { Write-Host ("ERROR: " + $_.Exception.Message) -ForegroundColor Red }
                continue
            }
            if ($Choice -eq '7') {
                try {
                    [void](Read-CojvrRelease $PSScriptRoot)
                    Assert-GameClosed
                    . (Join-Path $PSScriptRoot 'tools/coj_resolution_settings.ps1')
                    Show-PublicResolutionSelector
                } catch { Write-Host ("ERROR: " + $_.Exception.Message) -ForegroundColor Red }
                continue
            }
            $Requested = @{'1'='Install';'2'='Uninstall';'3'='Diagnose';'4'='Status';'5'='Verify';'6'='PrepareSession'}[$Choice]
            if ($null -eq $Requested) { continue }
            try { Invoke-PublicAction $Requested $Game $ManualReload.IsPresent $PSBoundParameters.ContainsKey('ManualReload') } catch { Write-Host ("ERROR: " + $_.Exception.Message) -ForegroundColor Red }
        }
    }
} catch { Write-Error $_; exit 1 }
