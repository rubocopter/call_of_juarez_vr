#Requires -Version 5.1
param(
    [switch]$LibraryOnly,
    [string]$GameDirectory = '',
    [string]$VideoSettingsPath = '',
    [string]$ResolutionSettingsPath = ''
)

$ErrorActionPreference = 'Stop'

# Encode a fixed, explicitly admitted invocation. Quoting is PowerShell literal
# quoting, not shell interpolation: folders can contain quotes, Unicode or $().
function New-CojvrGuiCommand {
    param(
        [string]$ScriptPath,
        [string]$Action,
        [string]$GameDirectory = '',
        [ValidateSet('Preserve','On','Off')][string]$ManualReloadChoice = 'Preserve',
        [string]$ResolutionProfile = '',
        [int]$ResolutionWidth = 0,
        [int]$ResolutionHeight = 0,
        [string]$VideoSettingsPath = '',
        [string]$ResolutionSettingsPath = ''
    )
    $Actions = @('Verify','Install','Uninstall','Diagnose','Status','PrepareSession','ConfigureResolution')
    if ($Action -cnotin $Actions) { throw 'Unsupported graphical launcher action.' }
    if ([string]::IsNullOrWhiteSpace($ScriptPath)) { throw 'Missing installer script.' }
    if ($Action -in @('Verify','ConfigureResolution')) {
        if (-not [string]::IsNullOrWhiteSpace($GameDirectory)) { throw 'This action does not accept a game directory.' }
    } elseif ([string]::IsNullOrWhiteSpace($GameDirectory)) {
        throw 'Select the Call of Juarez (2006) game folder first.'
    }
    if ($Action -ne 'Install' -and $ManualReloadChoice -ne 'Preserve') {
        throw 'Manual reload can only be changed during Install/update.'
    }
    if ($Action -ne 'ConfigureResolution') {
        if ($ResolutionProfile -ne '' -or $ResolutionWidth -ne 0 -or $ResolutionHeight -ne 0) {
            throw 'Resolution settings are only accepted by ConfigureResolution.'
        }
    } else {
        if ($ResolutionProfile -cnotin @('Preserve','Auto','75%','100%','125%','Manual')) {
            throw 'Unknown resolution profile.'
        }
        if ($ResolutionProfile -eq 'Manual') {
            if ($ResolutionWidth -lt 1 -or $ResolutionHeight -lt 1 -or
                $ResolutionWidth -gt 16384 -or $ResolutionHeight -gt 16384) {
                throw 'Manual resolution dimensions must be between 1 and 16384.'
            }
        } elseif ($ResolutionWidth -ne 0 -or $ResolutionHeight -ne 0) {
            throw 'Only Manual accepts width and height.'
        }
    }
    $Quote = { param([string]$Value) "'" + $Value.Replace("'", "''") + "'" }
    $Invocation = '& ' + (& $Quote $ScriptPath) + ' -Action ' + (& $Quote $Action)
    if ($Action -notin @('Verify','ConfigureResolution')) {
        $Invocation += ' -GameDirectory ' + (& $Quote $GameDirectory)
    }
    if ($Action -eq 'Install' -and $ManualReloadChoice -ne 'Preserve') {
        $Invocation += if ($ManualReloadChoice -eq 'On') { ' -ManualReload:$true' } else { ' -ManualReload:$false' }
    }
    if ($Action -eq 'ConfigureResolution') {
        $Invocation += ' -ResolutionProfile ' + (& $Quote $ResolutionProfile)
        if ($ResolutionProfile -eq 'Manual') {
            $Invocation += " -ResolutionWidth $ResolutionWidth -ResolutionHeight $ResolutionHeight"
        }
    }
    if ($VideoSettingsPath -ne '') { $Invocation += ' -VideoSettingsPath ' + (& $Quote $VideoSettingsPath) }
    if ($ResolutionSettingsPath -ne '') { $Invocation += ' -ResolutionSettingsPath ' + (& $Quote $ResolutionSettingsPath) }
    # The redirected Windows PowerShell progress/error streams otherwise arrive
    # as CLIXML instead of readable text in the graphical log.
    $Invocation = '$ProgressPreference = ''SilentlyContinue''; [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false); $global:LASTEXITCODE = 0; try { ' +
        $Invocation + ' *>&1 | ForEach-Object { [Console]::WriteLine([string]$_) }; exit $LASTEXITCODE } catch { [Console]::WriteLine(''ERROR: '' + $_.Exception.Message); exit 1 }'
    $Encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($Invocation))
    return '-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -OutputFormat Text -EncodedCommand ' + $Encoded
}

if ($LibraryOnly) { return }

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()

$Script = Join-Path $PSScriptRoot 'cojvr.ps1'
$PowerShellExe = Join-Path $PSHOME 'powershell.exe'
if (-not (Test-Path -LiteralPath $Script) -or -not (Test-Path -LiteralPath $PowerShellExe)) {
    [System.Windows.Forms.MessageBox]::Show('The complete release package and Windows PowerShell 5.1 are required.', 'CoJ VR installer') | Out-Null
    exit 1
}

$Form = New-Object System.Windows.Forms.Form
$Form.Text = 'Call of Juarez VR - Setup'
$Form.ClientSize = New-Object Drawing.Size(960,840)
$Form.MinimumSize = New-Object Drawing.Size(880,810)
$Form.StartPosition = 'CenterScreen'
$Form.AutoScaleMode = [Windows.Forms.AutoScaleMode]::Dpi
$Form.Font = New-Object Drawing.Font('Segoe UI',10)
$Form.BackColor = [Drawing.ColorTranslator]::FromHtml('#EFE4D0')
$Form.ForeColor = [Drawing.ColorTranslator]::FromHtml('#33251B')

$Ink = [Drawing.ColorTranslator]::FromHtml('#33251B')
$Leather = [Drawing.ColorTranslator]::FromHtml('#392820')
$Parchment = [Drawing.ColorTranslator]::FromHtml('#FAF2E3')
$Copper = [Drawing.ColorTranslator]::FromHtml('#A96F34')
$Border = [Drawing.ColorTranslator]::FromHtml('#AE9473')
$Muted = [Drawing.ColorTranslator]::FromHtml('#725B44')

function Set-CojvrButtonStyle {
    param([Windows.Forms.Button]$Button,[switch]$Primary,[switch]$Danger)
    $Button.FlatStyle = [Windows.Forms.FlatStyle]::Flat
    $Button.FlatAppearance.BorderSize = 1
    $Button.UseVisualStyleBackColor = $false
    $Button.Cursor = [Windows.Forms.Cursors]::Hand
    $Button.Font = New-Object Drawing.Font('Segoe UI',10,[Drawing.FontStyle]::Bold)
    $Button.BackColor = $Parchment
    $Button.ForeColor = $Leather
    $Button.FlatAppearance.BorderColor = $Border
    $Button.FlatAppearance.MouseOverBackColor = [Drawing.ColorTranslator]::FromHtml('#E8D6B6')
    if ($Primary) {
        $Button.BackColor = $Leather
        $Button.ForeColor = [Drawing.ColorTranslator]::FromHtml('#FFF1D4')
        $Button.FlatAppearance.BorderColor = $Copper
        $Button.FlatAppearance.MouseOverBackColor = [Drawing.ColorTranslator]::FromHtml('#65412A')
    }
    if ($Danger) {
        $Button.ForeColor = [Drawing.ColorTranslator]::FromHtml('#843E32')
        $Button.FlatAppearance.BorderColor = [Drawing.ColorTranslator]::FromHtml('#B99780')
    }
}

# Draw the frontier illustration ourselves so the player ZIP contains no game
# art or commercial assets. GDI+ redraws it when the user resizes the window.
$Hero = New-Object Windows.Forms.Panel
$Hero.Location = New-Object Drawing.Point(0,0)
$Hero.Size = New-Object Drawing.Size(960,164)
$Hero.Anchor = 'Top,Left,Right'
$Form.Controls.Add($Hero)
$Hero.Add_Paint({
    param($Sender,$EventArgs)
    if ($Sender.ClientSize.Width -le 0) { return }
    $G = $EventArgs.Graphics
    $State = $G.Save()
    $Resources = New-Object 'System.Collections.Generic.List[System.IDisposable]'
    try {
        $G.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $G.ScaleTransform([single]($Sender.ClientSize.Width / 960.0),[single]($Sender.ClientSize.Height / 164.0))
        $Background = New-Object Drawing.Drawing2D.LinearGradientBrush -ArgumentList @(
            [Drawing.Rectangle]::new(0,0,960,164),
            [Drawing.ColorTranslator]::FromHtml('#281D1B'),
            [Drawing.ColorTranslator]::FromHtml('#B56E3B'),
            [single]0)
        $Resources.Add($Background)
        $G.FillRectangle($Background,0,0,960,164)
        $Sun = New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml('#E4A76D'))
        $Far = New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml('#8B5138'))
        $Near = New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml('#4B3027'))
        $Dark = New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml('#261D1B'))
        $Line = New-Object Drawing.Pen -ArgumentList @($Dark,[single]3)
        $Cream = New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml('#FFE6B2'))
        $Gold = New-Object Drawing.SolidBrush ([Drawing.ColorTranslator]::FromHtml('#DFA85B'))
        $Resources.Add($Sun);$Resources.Add($Far);$Resources.Add($Near);$Resources.Add($Dark);$Resources.Add($Line);$Resources.Add($Cream);$Resources.Add($Gold)
        $G.FillEllipse($Sun,690,14,142,142)
        $G.FillPolygon($Far,[Drawing.Point[]]@(
            [Drawing.Point]::new(410,137),[Drawing.Point]::new(502,99),[Drawing.Point]::new(575,104),
            [Drawing.Point]::new(575,69),[Drawing.Point]::new(639,69),[Drawing.Point]::new(645,113),
            [Drawing.Point]::new(729,116),[Drawing.Point]::new(780,77),[Drawing.Point]::new(841,83),
            [Drawing.Point]::new(882,118),[Drawing.Point]::new(960,109),[Drawing.Point]::new(960,164),
            [Drawing.Point]::new(410,164)))
        $G.FillPolygon($Near,[Drawing.Point[]]@(
            [Drawing.Point]::new(380,164),[Drawing.Point]::new(512,131),[Drawing.Point]::new(635,144),
            [Drawing.Point]::new(737,117),[Drawing.Point]::new(849,134),[Drawing.Point]::new(960,112),
            [Drawing.Point]::new(960,164)))
        # A small original frontier scene: a windmill, a cactus and a trail.
        $G.FillRectangle($Dark,861,67,5,86)
        $G.DrawLine($Line,842,92,886,72)
        $G.DrawLine($Line,850,67,880,104)
        $G.FillRectangle($Dark,915,111,8,53)
        $G.FillRectangle($Dark,902,124,15,7)
        $G.FillRectangle($Dark,900,116,6,18)
        $G.FillRectangle($Dark,925,132,11,7)
        $G.FillRectangle($Dark,932,118,6,22)
        $G.FillRectangle($Dark,0,157,960,7)
        $HeroTitle = New-Object Drawing.Font('Georgia',28,[Drawing.FontStyle]::Bold)
        $HeroSubtitle = New-Object Drawing.Font('Georgia',17,[Drawing.FontStyle]::Bold)
        $HeroDetail = New-Object Drawing.Font('Segoe UI',9,[Drawing.FontStyle]::Bold)
        $Resources.Add($HeroTitle);$Resources.Add($HeroSubtitle);$Resources.Add($HeroDetail)
        $G.DrawString('CALL OF JUAREZ',$HeroTitle,$Cream,[single]29,[single]21)
        $G.DrawString('VIRTUAL REALITY',$HeroSubtitle,$Gold,[single]32,[single]82)
        $G.DrawString('COMMUNITY PRE-ALPHA    /    STEAMVR + D3D9',$HeroDetail,$Cream,[single]34,[single]132)
    } finally {
        foreach ($Resource in $Resources) { $Resource.Dispose() }
        $G.Restore($State)
    }
})

$Guide = New-Object System.Windows.Forms.Label
$Guide.Text = '01  Find the Steam / GOG game (or Browse)    02  Verify package    03  Install / update    04  Prepare each play session.  Back up saves, close the game and start SteamVR manually before installing or preparing.'
$Guide.Location = New-Object Drawing.Point(24,177)
$Guide.Size = New-Object Drawing.Size(910,49)
$Guide.Font = New-Object Drawing.Font('Segoe UI',9.5)
$Guide.ForeColor = $Muted
$Guide.Anchor = 'Top,Left,Right'
$Form.Controls.Add($Guide)

$FolderLabel = New-Object System.Windows.Forms.Label
$FolderLabel.Text = 'GAME DIRECTORY  /  ORIGINAL CALL OF JUAREZ (2006)'
$FolderLabel.Font = New-Object Drawing.Font('Segoe UI',9,[Drawing.FontStyle]::Bold)
$FolderLabel.ForeColor = $Leather
$FolderLabel.Location = New-Object Drawing.Point(24,236)
$FolderLabel.AutoSize = $true
$Form.Controls.Add($FolderLabel)

$FolderStatus = New-Object System.Windows.Forms.Label
$FolderStatus.Text = 'Locating Steam / GOG installations...'
$FolderStatus.Font = New-Object Drawing.Font('Segoe UI',9)
$FolderStatus.ForeColor = $Muted
$FolderStatus.Location = New-Object Drawing.Point(505,234)
$FolderStatus.Size = New-Object Drawing.Size(429,22)
$FolderStatus.AutoEllipsis = $true
$FolderStatus.TextAlign = 'MiddleRight'
$FolderStatus.Anchor = 'Top,Right'
$Form.Controls.Add($FolderStatus)

$FolderBox = New-Object System.Windows.Forms.ComboBox
$FolderBox.DropDownStyle = 'DropDown'
$FolderBox.Location = New-Object Drawing.Point(24,262)
$FolderBox.Size = New-Object Drawing.Size(688,29)
$FolderBox.Font = New-Object Drawing.Font('Segoe UI',10)
$FolderBox.FlatStyle = 'Flat'
$FolderBox.BackColor = $Parchment
$FolderBox.ForeColor = $Ink
$FolderBox.Anchor = 'Top,Left,Right'
$Form.Controls.Add($FolderBox)

$FindGame = New-Object System.Windows.Forms.Button
$FindGame.Text = 'Find game'
$FindGame.Location = New-Object Drawing.Point(724,260)
$FindGame.Size = New-Object Drawing.Size(105,35)
$FindGame.Anchor = 'Top,Right'
$FindGame.TabIndex = 1
$Form.Controls.Add($FindGame)
Set-CojvrButtonStyle $FindGame

$Browse = New-Object System.Windows.Forms.Button
$Browse.Text = 'Browse...'
$Browse.Location = New-Object Drawing.Point(835,260)
$Browse.Size = New-Object Drawing.Size(101,35)
$Browse.Anchor = 'Top,Right'
$Browse.TabIndex = 2
$FolderBox.TabIndex = 0
$Form.Controls.Add($Browse)
Set-CojvrButtonStyle $Browse
$Browse.Add_Click({
    $Dialog = New-Object System.Windows.Forms.FolderBrowserDialog
    $Dialog.Description = 'Choose the folder containing the original CoJ.exe'
    if (Test-Path -LiteralPath $FolderBox.Text -PathType Container) { $Dialog.SelectedPath = $FolderBox.Text }
    try { if ($Dialog.ShowDialog($Form) -eq 'OK') { $FolderBox.Text = $Dialog.SelectedPath } }
    finally { $Dialog.Dispose() }
})

$FolderTip = New-Object Windows.Forms.ToolTip
$FolderTip.SetToolTip($FolderBox,'Steam / GOG installations appear in this list. You may type or Browse for another folder.')
$FolderTip.SetToolTip($FindGame,'Scan Steam libraries and GOG installations again; no game files are changed.')
$FolderTip.SetToolTip($Browse,'Choose the folder containing CoJ.exe and ChromeEngine3.dll.')

$Group = New-Object System.Windows.Forms.GroupBox
$Group.Text = '  INSTALLATION  /  RECOVERY'
$Group.Font = New-Object Drawing.Font('Segoe UI',9,[Drawing.FontStyle]::Bold)
$Group.ForeColor = $Leather
$Group.BackColor = $Parchment
$Group.Location = New-Object Drawing.Point(22,307)
$Group.Size = New-Object Drawing.Size(916,150)
$Group.Anchor = 'Top,Left,Right'
$Form.Controls.Add($Group)

$ActionGrid = New-Object System.Windows.Forms.TableLayoutPanel
$ActionGrid.Dock = 'Fill'
$ActionGrid.ColumnCount = 3
$ActionGrid.RowCount = 2
$ActionGrid.Padding = New-Object System.Windows.Forms.Padding(8,18,8,8)
for ($Index = 0; $Index -lt 3; $Index++) {
    [void]$ActionGrid.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle -ArgumentList @([Windows.Forms.SizeType]::Percent,[single]33.3333)))
}
for ($Index = 0; $Index -lt 2; $Index++) {
    [void]$ActionGrid.RowStyles.Add((New-Object System.Windows.Forms.RowStyle -ArgumentList @([Windows.Forms.SizeType]::Percent,[single]50)))
}
$Group.Controls.Add($ActionGrid)
$Actions = @(
    @('Verify package','Verify'), @('Install / update','Install'), @('Prepare play session','PrepareSession'),
    @('Installation status','Status'), @('Save diagnostics','Diagnose'), @('Uninstall / recover','Uninstall')
)
$ActionButtons = New-Object 'System.Collections.Generic.List[System.Windows.Forms.Button]'
for ($Index = 0; $Index -lt $Actions.Count; $Index++) {
    $Entry = $Actions[$Index]
    $Button = New-Object System.Windows.Forms.Button
    $Button.Text = $Entry[0]
    $Button.Tag = $Entry[1]
    $Button.Dock = 'Fill'
    $Button.Margin = New-Object System.Windows.Forms.Padding(4)
    $Button.TabIndex = 3 + $Index
    Set-CojvrButtonStyle $Button -Primary:($Entry[1] -eq 'Install') -Danger:($Entry[1] -eq 'Uninstall')
    $ActionGrid.Controls.Add($Button,($Index % 3),[int][Math]::Floor($Index / 3))
    $ActionButtons.Add($Button)
}
$ActionTips = @{
    Verify='Check that the downloaded release is intact. No game folder required.'
    Install='Install or update the VR mod while the game is closed.'
    PrepareSession='Prepare for the next launch. Start SteamVR first; launch the game manually afterward.'
    Status='Read the current installation and recovery state.'
    Diagnose='Save a support report ZIP locally, without game saves.'
    Uninstall='Restore the original game files using the recorded backups.'
}
foreach ($Button in $ActionButtons) { $FolderTip.SetToolTip($Button,$ActionTips[[string]$Button.Tag]) }

$Settings = New-Object System.Windows.Forms.GroupBox
$Settings.Text = '  DISPLAY  /  NEXT SESSION'
$Settings.Font = New-Object Drawing.Font('Segoe UI',9,[Drawing.FontStyle]::Bold)
$Settings.ForeColor = $Leather
$Settings.BackColor = $Parchment
$Settings.Location = New-Object Drawing.Point(22,469)
$Settings.Size = New-Object Drawing.Size(916,100)
$Settings.Anchor = 'Top,Left,Right'
$Form.Controls.Add($Settings)

$Resolution = New-Object System.Windows.Forms.ComboBox
$Resolution.DropDownStyle = 'DropDownList'
$Resolution.Location = New-Object Drawing.Point(16,37)
$Resolution.Size = New-Object Drawing.Size(191,31)
$Resolution.BackColor = $Parchment
$Resolution.Font = New-Object Drawing.Font('Segoe UI',10)
[void]$Resolution.Items.AddRange(@('Preserve','Auto','75%','100%','125%','Manual'))
$Resolution.SelectedItem = 'Auto'
$Settings.Controls.Add($Resolution)

$WidthBox = New-Object System.Windows.Forms.NumericUpDown
$WidthBox.Minimum = 1; $WidthBox.Maximum = 16384; $WidthBox.Value = 2804
$WidthBox.Location = New-Object Drawing.Point(225,37)
$WidthBox.Size = New-Object Drawing.Size(95,31)
$Settings.Controls.Add($WidthBox)
$XLabel = New-Object System.Windows.Forms.Label
$XLabel.Text = 'x'; $XLabel.AutoSize = $true
$XLabel.Location = New-Object Drawing.Point(334,41)
$Settings.Controls.Add($XLabel)
$HeightBox = New-Object System.Windows.Forms.NumericUpDown
$HeightBox.Minimum = 1; $HeightBox.Maximum = 16384; $HeightBox.Value = 2860
$HeightBox.Location = New-Object Drawing.Point(354,37)
$HeightBox.Size = New-Object Drawing.Size(95,31)
$Settings.Controls.Add($HeightBox)
$SaveResolution = New-Object System.Windows.Forms.Button
$SaveResolution.Text = 'Save resolution choice'
$SaveResolution.Location = New-Object Drawing.Point(642,30)
$SaveResolution.Size = New-Object Drawing.Size(257,42)
$SaveResolution.Anchor = 'Top,Right'
$Settings.Controls.Add($SaveResolution)
Set-CojvrButtonStyle $SaveResolution
$UpdateDimensions = { $Enabled = ($Resolution.SelectedItem -eq 'Manual'); $WidthBox.Enabled=$Enabled; $HeightBox.Enabled=$Enabled }
$Resolution.Add_SelectedIndexChanged($UpdateDimensions)
& $UpdateDimensions

$Advanced = New-Object System.Windows.Forms.GroupBox
$Advanced.Text = '  ADVANCED  /  RELOAD PREFERENCE'
$Advanced.Font = New-Object Drawing.Font('Segoe UI',9,[Drawing.FontStyle]::Bold)
$Advanced.ForeColor = $Leather
$Advanced.BackColor = $Parchment
$Advanced.Location = New-Object Drawing.Point(22,581)
$Advanced.Size = New-Object Drawing.Size(916,80)
$Advanced.Anchor = 'Top,Left,Right'
$Form.Controls.Add($Advanced)
$ReloadLabel = New-Object System.Windows.Forms.Label
$ReloadLabel.Text = 'Only used when you click Install/update:'
$ReloadLabel.Font = New-Object Drawing.Font('Segoe UI',9)
$ReloadLabel.Location = New-Object Drawing.Point(16,35)
$ReloadLabel.AutoSize = $true
$Advanced.Controls.Add($ReloadLabel)
$ReloadChoice = New-Object System.Windows.Forms.ComboBox
$ReloadChoice.DropDownStyle = 'DropDownList'
$ReloadChoice.Items.AddRange(@('Keep current (default On for new installs)','On - install manual reload','Off - native reload'))
$ReloadChoice.SelectedIndex = 0
$ReloadChoice.Font = New-Object Drawing.Font('Segoe UI',10)
$ReloadChoice.BackColor = $Parchment
$ReloadChoice.Location = New-Object Drawing.Point(329,29)
$ReloadChoice.Size = New-Object Drawing.Size(566,31)
$ReloadChoice.Anchor = 'Top,Left,Right'
$Advanced.Controls.Add($ReloadChoice)

$Progress = New-Object System.Windows.Forms.ProgressBar
$Progress.Location = New-Object Drawing.Point(22,676)
$Progress.Size = New-Object Drawing.Size(916,12)
$Progress.Anchor = 'Top,Left,Right'
$Form.Controls.Add($Progress)

$Output = New-Object System.Windows.Forms.TextBox
$Output.Multiline = $true
$Output.ReadOnly = $true
$Output.ScrollBars = 'Vertical'
$Output.Font = New-Object Drawing.Font('Consolas',9)
$Output.BackColor = $Leather
$Output.ForeColor = [Drawing.ColorTranslator]::FromHtml('#F4E2C5')
$Output.BorderStyle = 'FixedSingle'
$Output.Location = New-Object Drawing.Point(22,699)
$Output.Size = New-Object Drawing.Size(916,116)
$Output.Anchor = 'Top,Bottom,Left,Right'
$Form.Controls.Add($Output)

$script:GuiProcess = $null
$script:GuiLogFiles = @()
$script:GuiPositions = @(0,0)
$script:GuiAction = ''
$script:GuiBusy = $false
$Timer = New-Object System.Windows.Forms.Timer
$Timer.Interval = 300

function Write-GuiOutput([string]$Message) {
    $Output.AppendText($Message + [Environment]::NewLine)
    $Output.SelectionStart = $Output.TextLength
    $Output.ScrollToCaret()
}

function Sync-GuiLog {
    for ($Index = 0; $Index -lt 2; $Index++) {
        $Path = $script:GuiLogFiles[$Index]
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { continue }
        try {
            $Stream = [IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
            try {
                if ($Stream.Length -lt $script:GuiPositions[$Index]) { $script:GuiPositions[$Index] = 0 }
                [void]$Stream.Seek([long]$script:GuiPositions[$Index],[IO.SeekOrigin]::Begin)
                $Reader = New-Object IO.StreamReader($Stream,[Text.Encoding]::UTF8,$true,1024,$true)
                try { $Text = $Reader.ReadToEnd() } finally { $Reader.Dispose() }
                $script:GuiPositions[$Index] = $Stream.Position
            } finally { $Stream.Dispose() }
            if ($Text.Length -gt 0) { Write-GuiOutput ($Text.TrimEnd("`r","`n")) }
        } catch [IO.IOException] { }
    }
}

function Finish-GuiProcess {
    $Timer.Stop()
    $script:GuiProcess.WaitForExit()
    $script:GuiProcess.Refresh()
    Sync-GuiLog
    $ReportedExitCode = $script:GuiProcess.ExitCode
    $ExitCode = if ($null -eq $ReportedExitCode -or [string]::IsNullOrWhiteSpace([string]$ReportedExitCode)) {
        'unavailable'
    } else { [int]$ReportedExitCode }
    $script:GuiProcess.Dispose()
    $script:GuiProcess = $null
    foreach ($Path in $script:GuiLogFiles) { Remove-Item -LiteralPath $Path -Force -ErrorAction SilentlyContinue }
    $script:GuiLogFiles = @()
    $script:GuiBusy = $false
    $Progress.Style = 'Blocks'
    foreach ($Button in $ActionButtons) { $Button.Enabled = $true }
    $SaveResolution.Enabled = $true
    $Browse.Enabled = $true
    $FindGame.Enabled = $true
    $FolderBox.Enabled = $true
    $Resolution.Enabled = $true
    $ReloadChoice.Enabled = $true
    & $UpdateDimensions
    if ($ExitCode -eq 0) {
        Write-GuiOutput "$($script:GuiAction) finished successfully."
    } else {
        Write-GuiOutput "$($script:GuiAction) failed (exit code $ExitCode). Review the output above."
    }
    if ($ExitCode -eq 0) {
        switch ($script:GuiAction) {
            'Install' { Write-GuiOutput 'Next: start SteamVR manually, save your resolution choice if needed, then select Prepare play session.' }
            'PrepareSession' { Write-GuiOutput 'Session prepared. Start Call of Juarez manually in D3D9 mode.' }
            'Diagnose' { Write-GuiOutput 'The report location appears above. Review its contents before sharing it.' }
        }
    }
}

$Timer.Add_Tick({
    if ($null -eq $script:GuiProcess) { return }
    Sync-GuiLog
    if ($script:GuiProcess.HasExited) { Finish-GuiProcess }
})

function Start-GuiAction([string]$Action) {
    if ($script:GuiBusy) { return }
    try {
        $Folder = $FolderBox.Text.Trim()
        if ($Action -notin @('Verify','ConfigureResolution')) {
            if ([string]::IsNullOrWhiteSpace($Folder) -or -not (Test-Path -LiteralPath $Folder -PathType Container)) {
                throw 'Choose the original Call of Juarez game folder first.'
            }
        } else { $Folder = '' }
        if ($Action -eq 'Uninstall') {
            $Choice = [Windows.Forms.MessageBox]::Show($Form,'Restore the original game files and remove the VR mod? Keep backups if any error occurs.','Uninstall / recover','YesNo','Warning')
            if ($Choice -ne 'Yes') { return }
        }
        $Params = @{ ScriptPath=(Get-CojvrGuiActionScript $PSScriptRoot $Action); Action=$Action; GameDirectory=$Folder
            VideoSettingsPath=$VideoSettingsPath; ResolutionSettingsPath=$ResolutionSettingsPath }
        if ($Action -eq 'Install') {
            $Params.ManualReloadChoice = @('Preserve','On','Off')[$ReloadChoice.SelectedIndex]
        }
        if ($Action -eq 'ConfigureResolution') {
            $Params.ResolutionProfile = [string]$Resolution.SelectedItem
            if ($Resolution.SelectedItem -eq 'Manual') {
                $Params.ResolutionWidth = [int]$WidthBox.Value
                $Params.ResolutionHeight = [int]$HeightBox.Value
            }
        }
        $Arguments = New-CojvrGuiCommand @Params
        $Stdout = Join-Path ([IO.Path]::GetTempPath()) ('cojvr-gui-' + [Guid]::NewGuid().ToString('N') + '.stdout.log')
        $Stderr = $Stdout.Replace('.stdout.log','.stderr.log')
        $Output.Clear()
        Write-GuiOutput "Running $Action..."
        $script:GuiProcess = Start-Process -FilePath $PowerShellExe -ArgumentList $Arguments -RedirectStandardOutput $Stdout -RedirectStandardError $Stderr -WindowStyle Hidden -PassThru
        # Retain the native handle before the child can exit. Windows PowerShell
        # may otherwise lose ExitCode for a short-lived asynchronous process.
        $null = $script:GuiProcess.Handle
        $script:GuiLogFiles = @($Stdout,$Stderr)
        $script:GuiPositions = @(0,0)
        $script:GuiAction = $Action
        $script:GuiBusy = $true
        foreach ($Button in $ActionButtons) { $Button.Enabled = $false }
        $SaveResolution.Enabled = $false
        $Browse.Enabled = $false
        $FindGame.Enabled = $false
        $FolderBox.Enabled = $false
        $Resolution.Enabled = $false
        $WidthBox.Enabled = $false
        $HeightBox.Enabled = $false
        $ReloadChoice.Enabled = $false
        $Progress.Style = 'Marquee'
        $Timer.Start()
    } catch {
        Write-GuiOutput ('ERROR: ' + $_.Exception.Message)
        [Windows.Forms.MessageBox]::Show($Form,$_.Exception.Message,'CoJ VR installer','OK','Error') | Out-Null
    }
}

foreach ($Button in $ActionButtons) {
    $Button.Add_Click({ param($Sender,$EventArgs) Start-GuiAction ([string]$Sender.Tag) })
}
$SaveResolution.Add_Click({ Start-GuiAction 'ConfigureResolution' })

. (Join-Path $PSScriptRoot 'release_common.ps1')
if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'release-manifest.json') -PathType Leaf)) {
    $Guide.Text = 'LOCAL INSTALLER  /  A verified player package is prepared automatically when needed. Choose the game folder, then Install / update. Start SteamVR and the game manually.'
    Write-GuiOutput 'Local installer ready. The first action prepares a verified package; progress appears here.'
}
$script:DetectedGames = @()
function Update-GuiFolderStatus {
    $Path = $FolderBox.Text.Trim()
    if ([string]::IsNullOrWhiteSpace($Path)) {
        if ($script:DetectedGames.Count -gt 1) {
            $FolderStatus.Text = "$($script:DetectedGames.Count) INSTALLS  /  choose from list"
            $FolderStatus.ForeColor = $Copper
        } elseif ($script:DetectedGames.Count -eq 0) {
            $FolderStatus.Text = 'Steam / GOG not found  /  use Browse'
            $FolderStatus.ForeColor = $Muted
        } else {
            $FolderStatus.Text = 'Select the original game folder'
            $FolderStatus.ForeColor = $Muted
        }
    } elseif ($Path -in $script:DetectedGames) {
        $FolderStatus.Text = 'GAME FOUND  /  build checked on install'
        $FolderStatus.ForeColor = [Drawing.ColorTranslator]::FromHtml('#37634C')
    } else {
        # Users can type incomplete or syntactically invalid paths. A status
        # hint must never surface an unhandled exception in the WinForms loop.
        $Found = $false
        try {
            $Found = (Test-Path -LiteralPath (Join-Path $Path 'CoJ.exe') -PathType Leaf -ErrorAction Stop) -and
                     (Test-Path -LiteralPath (Join-Path $Path 'ChromeEngine3.dll') -PathType Leaf -ErrorAction Stop)
        } catch { }
        if ($Found) {
            $FolderStatus.Text = 'GAME FILES FOUND  /  build checked on install'
            $FolderStatus.ForeColor = [Drawing.ColorTranslator]::FromHtml('#37634C')
        } else {
            $FolderStatus.Text = 'Choose a folder containing CoJ.exe'
            $FolderStatus.ForeColor = [Drawing.ColorTranslator]::FromHtml('#9A4536')
        }
    }
}
function Find-GuiGame {
    if ($script:GuiBusy) { return }
    try {
        $Previous = $FolderBox.Text.Trim()
        $script:DetectedGames = @(Get-CojvrPublicGameCandidates)
        $FolderBox.Items.Clear()
        foreach ($Path in $script:DetectedGames) { [void]$FolderBox.Items.Add($Path) }
        if ($script:DetectedGames.Count -eq 1) {
            $FolderBox.Text = $script:DetectedGames[0]
            Write-GuiOutput 'Game detected automatically (Steam / GOG). Verify package, then install or prepare a session.'
        } elseif ($script:DetectedGames.Count -gt 1) {
            $FolderBox.Text = if ($Previous -in $script:DetectedGames) { $Previous } else { '' }
            Write-GuiOutput "$($script:DetectedGames.Count) Steam / GOG game folders found. Choose the correct installation using the folder drop-down."
        } else {
            $FolderBox.Text = $Previous
            Write-GuiOutput 'Steam / GOG game not found automatically. Use Browse or enter the game folder; package verification works without it.'
        }
        Update-GuiFolderStatus
    } catch {
        Write-GuiOutput ('Game search failed: ' + $_.Exception.Message + '. Use Browse to select the folder.')
    }
}
$FolderBox.Add_TextChanged({ Update-GuiFolderStatus })
$FindGame.Add_Click({ Find-GuiGame })
Find-GuiGame
if (-not [string]::IsNullOrWhiteSpace($GameDirectory)) {
    $FolderBox.SelectedIndex = -1
    $FolderBox.Text = $GameDirectory
}

try {
    . (Get-CojvrResolutionSettingsHelper $PSScriptRoot)
    $PreferencePath = if ($ResolutionSettingsPath -ne '') { $ResolutionSettingsPath } else { Get-CoJResolutionSettingsPath }
    $SavedResolution = Read-CoJResolutionSettings -Path $PreferencePath
    if ($SavedResolution.profile -eq 'Manual' -and
        ($SavedResolution.width -gt [int]$WidthBox.Maximum -or
         $SavedResolution.height -gt [int]$HeightBox.Maximum)) {
        throw 'Saved Manual dimensions exceed the graphical editor limits. The existing setting is unchanged.'
    }
    $Resolution.SelectedItem = $SavedResolution.profile
    if ($SavedResolution.profile -eq 'Manual') {
        $WidthBox.Value = [decimal]$SavedResolution.width
        $HeightBox.Value = [decimal]$SavedResolution.height
    }
    $Extent = if ($SavedResolution.profile -eq 'Manual') { " $($SavedResolution.width)x$($SavedResolution.height)" } else { '' }
    Write-GuiOutput "Current saved resolution: $($SavedResolution.profile)$Extent"
} catch { Write-GuiOutput ('Resolution preference could not be displayed: ' + $_.Exception.Message) }

$Form.Add_FormClosing({
    param($Sender,$EventArgs)
    if ($script:GuiBusy) {
        [Windows.Forms.MessageBox]::Show($Form,'An operation is running. Let it finish before closing; interrupting installation may require recovery.','CoJ VR installer','OK','Warning') | Out-Null
        $EventArgs.Cancel = $true
    }
})
try { [void][Windows.Forms.Application]::Run($Form) }
finally { $Timer.Dispose(); $FolderTip.Dispose(); $Form.Dispose() }
