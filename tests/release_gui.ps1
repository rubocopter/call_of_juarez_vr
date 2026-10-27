param([string]$SourceDirectory = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Assert-True([bool]$Value,[string]$Message) { if (-not $Value) { throw $Message } }
function Assert-Rejects([scriptblock]$Block,[string]$Message) {
    $Thrown=$false
    try { & $Block | Out-Null } catch { $Thrown=$true }
    Assert-True $Thrown $Message
}
. (Join-Path $SourceDirectory 'release/cojvr-gui.ps1') -LibraryOnly
. (Join-Path $SourceDirectory 'release/release_common.ps1')
# The entry point used by the operator must resolve a complete backend instead
# of rejecting every action merely because it was opened from the repository.
foreach ($BackendAction in @('Verify','Install','PrepareSession','Status','Diagnose','Uninstall')) {
    $Backend = Get-CojvrGuiActionScript -LauncherDirectory (Join-Path $SourceDirectory 'release') -Action $BackendAction
    Assert-True ($Backend -eq (Join-Path $SourceDirectory 'tools/cojvr_source_action.ps1')) "Source GUI action $BackendAction has no usable package backend."
}
Assert-True ((Get-CojvrGuiActionScript -LauncherDirectory (Join-Path $SourceDirectory 'release') -Action ConfigureResolution) -eq (Join-Path $SourceDirectory 'release/cojvr.ps1')) 'Preference save must remain available without packaging.'
$Game="C:\Games\O'Brien `$(Write-Error unexpected); spaces & á"
$Cmd=New-CojvrGuiCommand -ScriptPath "C:\Folder\the'installer.ps1" -Action Install -GameDirectory $Game -ManualReloadChoice Off
Assert-True ($Cmd -cmatch '^-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -OutputFormat Text -EncodedCommand [A-Za-z0-9+/=]+$') 'Expected only a fixed encoded PowerShell launch.'
$Encoded=$Cmd.Split(' ')[-1]
$Decoded=[Text.Encoding]::Unicode.GetString([Convert]::FromBase64String($Encoded))
Assert-True ($Decoded.Contains("& 'C:\Folder\the''installer.ps1' -Action 'Install' -GameDirectory 'C:\Games\O''Brien")) 'Literal-quoted game paths were altered.'
Assert-True ($Decoded.Contains('-ManualReload:$false')) 'Explicit Off did not reach installer.'
Assert-True (-not $Decoded.Contains(' -Action Menu')) 'Interactive mode was reachable.'
$Kept=[Text.Encoding]::Unicode.GetString([Convert]::FromBase64String((New-CojvrGuiCommand -ScriptPath C:\cojvr.ps1 -Action Install -GameDirectory 'C:\game' -ManualReloadChoice Preserve).Split(' ')[-1]))
Assert-True (-not $Kept.Contains('-ManualReload:')) 'Install/update must retain stored reload preference unless changed explicitly.'
$Manual=[Text.Encoding]::Unicode.GetString([Convert]::FromBase64String((New-CojvrGuiCommand -ScriptPath C:\cojvr.ps1 -Action ConfigureResolution -ResolutionProfile Manual -ResolutionWidth 2804 -ResolutionHeight 2860).Split(' ')[-1]))
Assert-True ($Manual.Contains('-ResolutionWidth 2804 -ResolutionHeight 2860')) 'Manual dimensions not forwarded.'
Assert-Rejects { New-CojvrGuiCommand -ScriptPath C:\cojvr.ps1 -Action 'Menu' } 'Menu action was admitted.'
Assert-Rejects { New-CojvrGuiCommand -ScriptPath C:\cojvr.ps1 -Action 'Install;Remove-Item' } 'Unexpected actions were admitted.'
Assert-Rejects { New-CojvrGuiCommand -ScriptPath C:\cojvr.ps1 -Action 'ConfigureResolution' -ResolutionProfile Manual -ResolutionWidth -1 -ResolutionHeight 2860 } 'Invalid dimensions admitted.'
Assert-Rejects { New-CojvrGuiCommand -ScriptPath C:\cojvr.ps1 -Action Status -ResolutionProfile 'Manual' } 'Unused resolution arguments admitted.'
Assert-Rejects { New-CojvrGuiCommand -ScriptPath C:\cojvr.ps1 -Action Verify -GameDirectory 'C:\game' } 'Verify must not require a game.'
$Scoped = New-CojvrGuiCommand -ScriptPath C:\cojvr.ps1 -Action Status -GameDirectory C:\game -VideoSettingsPath "C:\O'Brien\Video.scr" -ResolutionSettingsPath 'C:\test\resolution.json'
$ScopedText=[Text.Encoding]::Unicode.GetString([Convert]::FromBase64String($Scoped.Split(' ')[-1]))
Assert-True ($ScopedText.Contains("-VideoSettingsPath 'C:\O''Brien\Video.scr'") -and $ScopedText.Contains("-ResolutionSettingsPath 'C:\test\resolution.json'")) 'Explicit isolated settings paths were not quoted/forwarded.'

# Exercise the same encoded, redirected process used by the GUI from a source
# checkout without a generated release manifest. Preferences are harmless;
# Install must refuse to touch an unpackaged build with a readable error.
$Scratch = Join-Path $SourceDirectory ('work/gui-launch-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $Scratch -Force | Out-Null
function Invoke-GuiFixture([string]$Action,[string]$GameDirectory = '') {
    $Parameters = @{ScriptPath=(Join-Path $SourceDirectory 'release/cojvr.ps1');Action=$Action;GameDirectory=$GameDirectory}
    if ($Action -eq 'ConfigureResolution') {
        $Parameters.ResolutionProfile = 'Manual'
        $Parameters.ResolutionWidth = 2804
        $Parameters.ResolutionHeight = 2860
    }
    $Arguments = New-CojvrGuiCommand @Parameters
    $OutFile = Join-Path $Scratch "$Action.out"
    $ErrFile = Join-Path $Scratch "$Action.err"
    $Process = Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') -ArgumentList $Arguments `
        -RedirectStandardOutput $OutFile -RedirectStandardError $ErrFile -PassThru -WindowStyle Hidden
    $null = $Process.Handle
    $Process.WaitForExit()
    $Process.Refresh()
    return [pscustomobject]@{
        ExitCode=$Process.ExitCode
        Output=[IO.File]::ReadAllText($OutFile)
        Error=[IO.File]::ReadAllText($ErrFile)
    }
}
try {
    $OriginalLocalAppData=$env:LOCALAPPDATA
    $OriginalModulePath=$env:PSModulePath
    try {
        $env:LOCALAPPDATA=$Scratch
        # Desktop hosts/PowerShell 7 can pass a different module path to 5.1.
        # The frontend must select its own utility/security modules explicitly.
        $env:PSModulePath=Join-Path $env:ProgramFiles 'PowerShell/7/Modules'
        $Result=Invoke-GuiFixture 'ConfigureResolution'
        $Preference=Join-Path $Scratch 'CoJVR/CallOfJuarez/resolution.json'
        Assert-True ($Result.ExitCode -eq 0 -and (Get-Content -LiteralPath $Preference -Raw | ConvertFrom-Json).width -eq 2804) "Source GUI child failed to save Manual: $($Result.Output) $($Result.Error)"
        Assert-True ($Result.Output -match 'Saved resolution profile: Manual' -and $Result.Error -notmatch 'CLIXML|<Objs|_x000D_') "Source preference child emitted a serialized PowerShell error: $($Result.Error)"
        $FakeGame=Join-Path $Scratch 'empty-game'
        New-Item -ItemType Directory -Path $FakeGame | Out-Null
        $Before=@(Get-ChildItem -LiteralPath $FakeGame -Force).Count
        $Result=Invoke-GuiFixture 'Install' $FakeGame
        Assert-True ($Result.ExitCode -ne 0 -and $Result.Output -match 'release.*(package|ZIP|manifest)' -and $Result.Error -notmatch 'CLIXML|<Objs|_x000D_') "Missing package produced an unreadable error: $($Result.Output) $($Result.Error)"
        Assert-True (@(Get-ChildItem -LiteralPath $FakeGame -Force).Count -eq $Before) 'Missing package check altered the game directory.'
        $Streams = Join-Path $Scratch 'streams.ps1'
        [IO.File]::WriteAllText($Streams, "param([string]`$Action)`nWrite-Warning 'warning text'`nWrite-Host 'informational text'`nWrite-Error 'expected error' -ErrorAction Continue`nexit 7`n")
        $Arguments = New-CojvrGuiCommand -ScriptPath $Streams -Action Verify
        $StreamProcess = Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') -ArgumentList $Arguments -RedirectStandardOutput (Join-Path $Scratch 'streams.out') -RedirectStandardError (Join-Path $Scratch 'streams.err') -PassThru -WindowStyle Hidden
        $null = $StreamProcess.Handle
        $StreamProcess.WaitForExit(); $StreamProcess.Refresh()
        $StreamText = [IO.File]::ReadAllText((Join-Path $Scratch 'streams.out')) + [IO.File]::ReadAllText((Join-Path $Scratch 'streams.err'))
        Assert-True ($StreamProcess.ExitCode -eq 7 -and $StreamText -notmatch 'CLIXML|<Objs|_x000D_' -and $StreamText -match 'warning text' -and $StreamText -match 'expected error') "Child streams/exit lost: $StreamText"
        $StreamProcess.Dispose()
    } finally { $env:LOCALAPPDATA=$OriginalLocalAppData; $env:PSModulePath=$OriginalModulePath }
} finally { Remove-Item -LiteralPath $Scratch -Recurse -Force }

# Execute the real process-completion handler without opening the GUI or touching
# any installed game. A parser-only check misses valid syntax that fails at runtime.
$Tokens = $null
$ParseErrors = $null
$GuiAst = [Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $SourceDirectory 'release/cojvr-gui.ps1'),[ref]$Tokens,[ref]$ParseErrors)
Assert-True ($ParseErrors.Count -eq 0) 'GUI script has parser errors.'
$CompletionFunctions = @($GuiAst.FindAll({
    param($Node)
    $Node -is [Management.Automation.Language.FunctionDefinitionAst] -and
        $Node.Name -eq 'Finish-GuiProcess'
},$true))
Assert-True ($CompletionFunctions.Count -eq 1) 'Expected one real GUI completion handler.'
. ([scriptblock]::Create($CompletionFunctions[0].Extent.Text))

function Sync-GuiLog { }
function Write-GuiOutput([string]$Message) { $script:CapturedMessages += $Message }
function New-FakeGuiControl { [pscustomobject]@{Enabled=$false} }

$Timer = [pscustomobject]@{Stopped=$false}
$Timer | Add-Member -MemberType ScriptMethod -Name Stop -Value { $this.Stopped = $true }
$Progress = [pscustomobject]@{Style='Marquee'}
$ActionButtons = @((New-FakeGuiControl),(New-FakeGuiControl))
$SaveResolution = New-FakeGuiControl
$Browse = New-FakeGuiControl
$FindGame = New-FakeGuiControl
$FolderBox = New-FakeGuiControl
$Resolution = New-FakeGuiControl
$ReloadChoice = New-FakeGuiControl
$WidthBox = New-FakeGuiControl
$HeightBox = New-FakeGuiControl
$UpdateDimensions = { $WidthBox.Enabled = $true; $HeightBox.Enabled = $true }

foreach ($Scenario in @(
    @{ ExitCode=0; Expected='ConfigureResolution finished successfully.' },
    @{ ExitCode=7; Expected='ConfigureResolution failed (exit code 7). Review the output above.' },
    @{ ExitCode=$null; Expected='ConfigureResolution failed (exit code unavailable). Review the output above.' }
)) {
    $script:GuiProcess = [pscustomobject]@{ExitCode=$Scenario.ExitCode; Disposed=$false}
    $script:GuiProcess | Add-Member -MemberType ScriptMethod -Name Dispose -Value { $this.Disposed = $true }
    $script:GuiProcess | Add-Member -MemberType ScriptMethod -Name WaitForExit -Value { }
    $script:GuiProcess | Add-Member -MemberType ScriptMethod -Name Refresh -Value { }
    $CompletedProcess = $script:GuiProcess
    $script:GuiLogFiles = @()
    $script:GuiAction = 'ConfigureResolution'
    $script:GuiBusy = $true
    $script:CapturedMessages = @()
    $Timer.Stopped = $false
    $Progress.Style = 'Marquee'
    Finish-GuiProcess
    Assert-True ($script:CapturedMessages.Count -eq 1 -and $script:CapturedMessages[0] -ceq $Scenario.Expected) "Unexpected completion output for exit $($Scenario.ExitCode)."
    Assert-True ($Timer.Stopped -and $CompletedProcess.Disposed -and $null -eq $script:GuiProcess -and -not $script:GuiBusy) 'Process resources or busy state were not cleared.'
    Assert-True ($Progress.Style -eq 'Blocks' -and $SaveResolution.Enabled -and $WidthBox.Enabled -and $HeightBox.Enabled -and $ActionButtons[0].Enabled) 'GUI controls did not recover after process exit.'
}
Write-Host 'PASS release GUI: safe arguments, profiles, malformed requests, completion callback (success/failure).'
