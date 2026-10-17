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
    $VideoFolder = Join-Path $Root ('User ' + [char]0x00E9)
    New-Item -ItemType Directory -Path $VideoFolder | Out-Null
    $Video = Join-Path $VideoFolder 'Video.scr'
    [IO.File]::WriteAllText($Video, "Resolution(1280,720)`nFSAA(4)`n")
    Invoke-Frontend @('-Action','Verify')
    Invoke-Frontend @('-Action','Install','-VideoSettingsPath',$Video)
    $Control = Get-Content -LiteralPath (Join-Path $Game 'cojvr-camera-control.json') -Raw | ConvertFrom-Json
    Assert-True ($Control.trackingEnabled -and $Control.bodyIkEnabled -and $Control.independentHandsEnabled) 'Public installation does not enable tracked head/body/independent hands.'
    $RunBefore = Get-Content -LiteralPath (Join-Path $Game '.cojvr-run.json') -Raw | ConvertFrom-Json
    [IO.File]::WriteAllText((Join-Path $Game 'cojvr.log'), 'previous run log')
    [IO.File]::WriteAllText((Join-Path $Game 'callstack.txt'), 'previous crash')
    Invoke-Frontend @('-Action','PrepareSession')
    $RunAfter = Get-Content -LiteralPath (Join-Path $Game '.cojvr-run.json') -Raw | ConvertFrom-Json
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
    Invoke-Frontend @('-Action','Install','-VideoSettingsPath',$Video,'-ManualReload')
    Invoke-Frontend @('-Action','Install','-VideoSettingsPath',$Video)
    $State = Get-Content -LiteralPath (Join-Path $Game '.cojvr-public-install.json') -Raw | ConvertFrom-Json
    Assert-True $State.manualReload 'Updating reset manual reload preference.'
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
    Write-Host 'PASS isolated exact-game install, active tracking, fresh sessions, all controllers, updates, preference preservation, uninstall and injected failure restoration. Host-only.'
} finally {
    $Allowed = [IO.Path]::GetFullPath((Join-Path $SourceDirectory 'work')).TrimEnd('\') + '\'
    $Resolved = [IO.Path]::GetFullPath($Root)
    if ($Resolved.StartsWith($Allowed, [StringComparison]::OrdinalIgnoreCase)) { Remove-Item -LiteralPath $Resolved -Recurse -Force }
}
