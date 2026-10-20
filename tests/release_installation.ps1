# Opt-in host integration. Copies four originals into an isolated lab; never
# starts the game/SteamVR and never stages the player's actual installation.
param(
    [Parameter(Mandatory=$true)][string]$PackageDirectory,
    [Parameter(Mandatory=$true)][string]$OriginalGameDirectory,
    [string]$SourceDirectory = ''
)
$ErrorActionPreference = 'Stop'
if (-not $SourceDirectory) { $SourceDirectory = Split-Path $PSScriptRoot -Parent }
$PackageDirectory = [IO.Path]::GetFullPath($PackageDirectory)
. (Join-Path $PackageDirectory 'release_common.ps1')
$Manifest = Read-CojvrRelease $PackageDirectory
$Runtime = Get-ItemProperty 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x86' -ErrorAction SilentlyContinue
if ($null -eq $Runtime -or $Runtime.Installed -ne 1 -or
    [version]([string]$Runtime.Version).TrimStart('v') -lt [version]$Manifest.dependencies.vcRuntimeVersion) {
    Write-Host 'Integration skipped: tests must not install a system VC++ runtime.'; exit 77
}
$Root = Join-Path $SourceDirectory ('work/release-installation-' + [guid]::NewGuid().ToString('N'))
$Game = Join-Path $Root ('Game ' + [char]0x00F1 + ' sandbox')
$Preference=Join-Path $Root 'user/resolution.json'
New-Item -ItemType Directory -Path $Game -Force | Out-Null
function Assert-True($Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Invoke-Frontend([string[]]$Arguments, [bool]$ShouldPass=$true) {
    & powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PackageDirectory 'cojvr.ps1') @Arguments -GameDirectory $Game -ResolutionSettingsPath $Preference
    Assert-True (($LASTEXITCODE -eq 0) -eq $ShouldPass) "Unexpected frontend exit code $LASTEXITCODE for $Arguments."
}
function Assert-Originals {
    foreach ($Name in $OriginalHashes.Keys) {
        Assert-True ((Get-FileHash -LiteralPath (Join-Path $Game $Name) -Algorithm SHA256).Hash -eq $OriginalHashes[$Name]) "Original $Name not restored."
    }
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $Game 'd3d9.dll'))) 'Proxy not removed.'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $Game '.cojvr-d3d9-stage.json'))) 'Staging still active.'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $Game '.cojvr-public-install.json'))) 'Public journal remains.'
    Assert-True (([IO.File]::ReadAllText($Video)) -eq "Resolution(1280,720)`nFSAA(4)`n") 'Video settings not restored.'
}
try {
    $OriginalHashes = @{}
    foreach ($Name in @('CoJ.exe','ChromeEngine3.dll','code.pak','Data0.pak')) {
        Copy-Item -LiteralPath (Join-Path $OriginalGameDirectory $Name) -Destination (Join-Path $Game $Name)
        $OriginalHashes[$Name] = (Get-FileHash -LiteralPath (Join-Path $Game $Name) -Algorithm SHA256).Hash
    }
    $VideoFolder = Join-Path $Root ('OneDrive - User ' + [char]0x00E9 + '/Documents/call of juarez/out/Settings')
    New-Item -ItemType Directory -Path $VideoFolder -Force | Out-Null
    $Video = Join-Path $VideoFolder 'Video.scr'
    [IO.File]::WriteAllText($Video, "Resolution(1280,720)`nFSAA(4)`n")
    Invoke-Frontend @('-Action','Verify')
    Invoke-Frontend @('-Action','Install','-VideoSettingsPath',$Video)
    $Control = Get-Content -LiteralPath (Join-Path $Game 'cojvr-camera-control.json') -Raw | ConvertFrom-Json
    Assert-True ($Control.trackingEnabled -and $Control.bodyIkEnabled -and $Control.independentHandsEnabled) 'Public installation does not enable tracked head/body/independent hands.'
    Assert-True $Control.manualReloadInstalled 'Default installation omitted manual reload.'
    $Python=Join-Path $PackageDirectory 'dependencies/python/python.exe'
    $MenuVerifier=Join-Path $Root 'verify_native_options.py'
    [IO.File]::WriteAllText($MenuVerifier, @'
import sys, zipfile
from pathlib import Path
from coj_native_settings import MENU_CLASS_NAME, MENU_SCRIPT_NAME, patch_menu_class, patch_menu_script
root = Path(sys.argv[1])
for archive, entry, transform in (
    ('code', MENU_CLASS_NAME, patch_menu_class),
    ('Data0', MENU_SCRIPT_NAME, patch_menu_script),
):
    with zipfile.ZipFile(root / (archive + '.cojvr-backup.pak')) as old, zipfile.ZipFile(root / (archive + '.pak')) as new:
        assert new.read(entry) == transform(old.read(entry))
'@)
    & $Python -B $MenuVerifier $Game
    Assert-True ($LASTEXITCODE -eq 0) 'Installed native options class/resources differ from the exact transformations.'
    $RunBefore = Get-Content -LiteralPath (Join-Path $Game '.cojvr-run.json') -Raw | ConvertFrom-Json
    Assert-True ($RunBefore.validation.manualReloadInstalled -and $RunBefore.validation.handPresentation -eq 'independent_native_hands') 'Default session omitted the complete hands/reload feature set.'
    Assert-True ($RunBefore.validation.videoProfile.resolutionProfile -eq 'Auto') 'First installation did not select automatic headset resolution.'
    [IO.File]::WriteAllText((Join-Path $Game 'cojvr.log'), 'previous run log')
    [IO.File]::WriteAllText((Join-Path $Game 'callstack.txt'), 'previous crash')
    Invoke-Frontend @('-Action','PrepareSession')
    $RunAfter = Get-Content -LiteralPath (Join-Path $Game '.cojvr-run.json') -Raw | ConvertFrom-Json
    Assert-True $RunAfter.validation.manualReloadInstalled 'Preparing another session lost manual reload.'
    Assert-True ($RunBefore.runId -ne $RunAfter.runId) 'Session identity reused.'
    $Stage = Get-Content -LiteralPath (Join-Path $Game '.cojvr-d3d9-stage.json') -Raw | ConvertFrom-Json
    Assert-True ($Stage.runId -eq $RunAfter.runId) 'Session/stage identity mismatch.'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $Game 'cojvr.log'))) 'Prior session log remains active.'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $Game 'callstack.txt'))) 'Prior crash remains active.'
    foreach($Extent in @(@(2804,2860),@(2428,2477))) {
        Invoke-Frontend @('-Action','ConfigureResolution','-ResolutionProfile','Manual','-ResolutionWidth',[string]$Extent[0],'-ResolutionHeight',[string]$Extent[1])
        Invoke-Frontend @('-Action','PrepareSession')
        $ProfileRun=Get-Content -LiteralPath (Join-Path $Game '.cojvr-run.json') -Raw | ConvertFrom-Json
        Assert-True ($ProfileRun.validation.videoProfile.resolution -eq "$($Extent[0])x$($Extent[1])") 'Selected startup extent was not applied.'
        Assert-True ((Get-Content $Video -Raw) -match ('Resolution\('+[string]$Extent[0]+','+[string]$Extent[1]+'\)')) 'Actual Video.scr differs from applied profile metadata.'
    }
    $Actions = Get-Content -LiteralPath (Join-Path $Game 'cojvr_openvr_input/actions.json') -Raw | ConvertFrom-Json
    Assert-True ($Actions.default_bindings.Count -ge 5) 'Additional controller defaults not installed.'
    foreach ($Binding in $Actions.default_bindings) {
        Assert-True (Test-Path -LiteralPath (Join-Path $Game ('cojvr_openvr_input/' + $Binding.binding_url))) 'Referenced controller binding missing.'
    }
    # An explicit conventional-reload installation must survive ordinary updates.
    & (Join-Path $PackageDirectory 'cojvr.ps1') -Action Install -GameDirectory $Game -VideoSettingsPath $Video -ResolutionSettingsPath $Preference -ManualReload:$false
    $State = Get-Content -LiteralPath (Join-Path $Game '.cojvr-public-install.json') -Raw | ConvertFrom-Json
    Assert-True (-not $State.manualReload) 'Explicit script reload override ignored.'
    # The ordinary menu must complete an update and exit with just "1, 0";
    # no separate reload question may consume the exit selection.
    . (Join-Path $PackageDirectory 'tools/coj_resolution_session.ps1')
    $Menu = Invoke-CoJResolutionProgram -ExecutablePath (Get-Command powershell.exe).Source -Arguments @(
        '-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $PackageDirectory 'cojvr.ps1'),
        '-GameDirectory',$Game,'-VideoSettingsPath',$Video,'-ResolutionSettingsPath',$Preference
    ) -StandardInput "1`n0`n"
    Assert-True ($Menu.ExitCode -eq 0) ('Default menu update failed: ' + $Menu.StdErr)
    $State = Get-Content -LiteralPath (Join-Path $Game '.cojvr-public-install.json') -Raw | ConvertFrom-Json
    Assert-True (-not $State.manualReload) 'Default update overwrote explicit reload-off preference.'
    $Control = Get-Content -LiteralPath (Join-Path $Game 'cojvr-camera-control.json') -Raw | ConvertFrom-Json
    Assert-True (-not $Control.manualReloadInstalled -and $Control.independentHandsEnabled -and $Control.bodyIkEnabled) 'Updated installation changed reload choice or omitted independent hands/body.'
    $UpdatedRun = Get-Content -LiteralPath (Join-Path $Game '.cojvr-run.json') -Raw | ConvertFrom-Json
    Assert-True ($UpdatedRun.validation.videoProfile.resolution -eq '2428x2477') 'Default feature update replaced the saved Manual resolution.'
    Invoke-Frontend @('-Action','PrepareSession')
    $Prepared = Get-Content -LiteralPath (Join-Path $Game '.cojvr-run.json') -Raw | ConvertFrom-Json
    Assert-True (-not $Prepared.validation.manualReloadInstalled) 'Prepare session enabled unselected manual reload.'
    $MenuArguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $PackageDirectory 'cojvr.ps1'),
        '-GameDirectory',$Game,'-VideoSettingsPath',$Video,'-ResolutionSettingsPath',$Preference)
    foreach ($Selection in @(@('1',$true),@('2',$false))) {
        $Menu = Invoke-CoJResolutionProgram -ExecutablePath (Get-Command powershell.exe).Source -Arguments $MenuArguments -StandardInput ("8`n" + $Selection[0] + "`n0`n")
        Assert-True ($Menu.ExitCode -eq 0 -and $Menu.StdOut -notmatch 'ERROR:') ('Reload selector failed: ' + $Menu.StdOut + $Menu.StdErr)
        $State = Get-Content -LiteralPath (Join-Path $Game '.cojvr-public-install.json') -Raw | ConvertFrom-Json
        $Control = Get-Content -LiteralPath (Join-Path $Game 'cojvr-camera-control.json') -Raw | ConvertFrom-Json
        Assert-True ($State.manualReload -eq $Selection[1] -and $Control.manualReloadInstalled -eq $Selection[1]) 'Menu selection did not install chosen reload mode.'
        Invoke-Frontend @('-Action','PrepareSession')
        $Prepared = Get-Content -LiteralPath (Join-Path $Game '.cojvr-run.json') -Raw | ConvertFrom-Json
        Assert-True ($Prepared.validation.manualReloadInstalled -eq $Selection[1]) 'Prepare session lost menu reload choice.'
    }
    $BeforeCancel = (Get-FileHash -LiteralPath (Join-Path $Game '.cojvr-public-install.json')).Hash
    $Menu = Invoke-CoJResolutionProgram -ExecutablePath (Get-Command powershell.exe).Source -Arguments $MenuArguments -StandardInput "8`n0`n0`n"
    Assert-True ($Menu.ExitCode -eq 0 -and (Get-FileHash -LiteralPath (Join-Path $Game '.cojvr-public-install.json')).Hash -eq $BeforeCancel) 'Cancelling reload selector changed installation.'
    Invoke-Frontend @('-Action','Uninstall')
    Assert-Originals
    $env:COJVR_DEPLOYMENT_FAILURE_TEST = '1'
    $env:COJVR_DEPLOYMENT_FAIL_AFTER = 'stage_proxy_published'
    try { Invoke-Frontend @('-Action','Install','-VideoSettingsPath',$Video) $false } finally {
        Remove-Item Env:COJVR_DEPLOYMENT_FAILURE_TEST -ErrorAction SilentlyContinue
        Remove-Item Env:COJVR_DEPLOYMENT_FAIL_AFTER -ErrorAction SilentlyContinue
    }
    Assert-Originals
    Invoke-Frontend @('-Action','Verify')
    Write-Host 'PASS isolated exact-game defaults, Auto resolution, reload on/off selector, preserved update/session preferences, all controllers, saved Manual size, uninstall and failure restoration. Host-only.'
} finally {
    $Allowed = [IO.Path]::GetFullPath((Join-Path $SourceDirectory 'work')).TrimEnd('\') + '\'
    $Resolved = [IO.Path]::GetFullPath($Root)
    if ($Resolved.StartsWith($Allowed, [StringComparison]::OrdinalIgnoreCase)) { Remove-Item -LiteralPath $Resolved -Recurse -Force }
}
