param(
    [Parameter(Mandatory = $true)] [string]$SourceDirectory,
    [Parameter(Mandatory = $true)] [string]$BinaryDirectory
)

$ErrorActionPreference = "Stop"
$TestRoot = Join-Path ([IO.Path]::GetFullPath($BinaryDirectory)) ("vr-video-profile-test-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $TestRoot | Out-Null

# Load only the profile boundary, never the prepare/finish entry point: these
# fixtures must not resolve Documents, build, stage or touch the real game.
$HelperPath = Join-Path $SourceDirectory 'tools/coj_video_profile.ps1'
if (Test-Path -LiteralPath $HelperPath) { . $HelperPath }
$Tokens = $null
$ParseErrors = $null
$Ast = [Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $SourceDirectory 'tools/vr_test.ps1'), [ref]$Tokens, [ref]$ParseErrors)
if ($ParseErrors.Count) { throw "vr_test.ps1 has parse errors: $ParseErrors" }
foreach ($Name in @('Apply-VrVideoProfile', 'Restore-VrVideoProfile')) {
    $Definition = $Ast.Find({ param($Node)
        $Node -is [Management.Automation.Language.FunctionDefinitionAst] -and $Node.Name -eq $Name
    }, $true)
    if (-not $Definition) { throw "Missing profile boundary: $Name" }
    Invoke-Expression $Definition.Extent.Text
}
function Get-CoJVideoSettingsPath { return $VideoPath }

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Assert-Rejected([scriptblock]$Operation, [string]$Message) {
    $Rejected = $false
    try { & $Operation | Out-Null } catch { $Rejected = $true }
    Assert-True $Rejected $Message
}
function New-Fixture([string]$Name, [string]$Text, [bool]$Bom = $false) {
    $script:LocalStateDirectory = Join-Path $TestRoot $Name
    New-Item -ItemType Directory -Path $LocalStateDirectory | Out-Null
    $script:VideoPath = Join-Path $LocalStateDirectory 'Video.scr'
    $script:VideoProfileStatePath = Join-Path $LocalStateDirectory 'vr-video-profile.json'
    $script:VideoProfileBackupPath = Join-Path $LocalStateDirectory 'vr-video-profile.backup'
    [IO.File]::WriteAllText($VideoPath, $Text, [Text.UTF8Encoding]::new($Bom))
    return (Get-FileHash -LiteralPath $VideoPath -Algorithm SHA256).Hash
}

try {
    # A deleted destination must be recreated from the verified original bytes.
    foreach ($BackupCondition in @('valid', 'missing', 'corrupt')) {
        $OriginalHash = New-Fixture "missing-video-$BackupCondition" "Resolution(1280,720)`r`nFSAA(4)`r`n" $true
        [void](Apply-VrVideoProfile)
        Remove-Item -LiteralPath $VideoPath
        if ($BackupCondition -eq 'missing') { Remove-Item -LiteralPath $VideoProfileBackupPath }
        if ($BackupCondition -eq 'corrupt') { [IO.File]::WriteAllText($VideoProfileBackupPath, 'corrupt') }
        if ($BackupCondition -eq 'valid') {
            Assert-True (Restore-VrVideoProfile) 'Missing Video.scr was not restored.'
            Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) 'Missing destination lost original bytes.'
            Assert-True (-not (Test-Path $VideoProfileStatePath) -and -not (Test-Path $VideoProfileBackupPath)) 'Missing destination recovery left a transaction.'
        } else {
            Assert-Rejected { Restore-VrVideoProfile } 'Missing destination accepted an invalid backup.'
            Assert-True (-not (Test-Path $VideoPath) -and (Test-Path $VideoProfileStatePath)) 'Invalid backup created a destination or discarded its journal.'
        }
    }

    $UnicodeHash = New-Fixture ('user-' + [char]0x00E9) "Resolution(1280,720)`nFSAA(4)`n"
    [void](Apply-VrVideoProfile)
    Assert-True (Restore-VrVideoProfile) 'Unicode video-path recovery failed.'
    Assert-True ((Get-FileHash -LiteralPath $VideoPath).Hash -eq $UnicodeHash) 'Unicode video settings changed after restore.'
    $Original = "!Resolution(i,i)`r`n!FSAA(i) // declaration`r`nResolution(2560,1440)`r`nFSAA(4)`r`nTextureQuality(`"Normal`")`r`nFiltering(`"AnisotropicTrilinear`")`r`nPostprocess(`"Normal`")`r`n"
    $OriginalHash = New-Fixture 'selected-1440p' $Original $true
    $Profile = Apply-VrVideoProfile
    $Applied = [IO.File]::ReadAllText($VideoPath)
    Assert-True ($Applied -eq $Original.Replace('FSAA(4)', 'FSAA(0)')) 'Profile changed selected resolution or unrelated quality settings.'
    Assert-True ($Profile.resolution -eq '2560x1440' -and $Profile.fsaa -eq 0) 'Profile provenance does not describe the actual settings.'
    Assert-True ($Profile.experimental -eq $false -and $Profile.requestedWidth -eq 0 -and $Profile.requestedHeight -eq 0) 'Default apply acquired experimental intent.'
    Assert-True ((Get-FileHash $VideoProfileBackupPath).Hash -eq $OriginalHash) 'Backup did not preserve original bytes.'
    Assert-True ((Get-FileHash $VideoPath).Hash -eq $Profile.appliedSha256) 'Applied provenance hash is incorrect.'
    Assert-Rejected { Apply-VrVideoProfile } 'A second apply overwrote an active transaction.'
    Assert-True (Restore-VrVideoProfile) 'Restore did not report an active transaction.'
    Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) 'Restore did not recover the original bytes/BOM/CRLF.'
    Assert-True (-not (Test-Path $VideoProfileBackupPath) -and -not (Test-Path $VideoProfileStatePath)) 'Successful restore left an active journal.'
    Assert-True (-not (Restore-VrVideoProfile)) 'Inactive restore should be a no-op.'

    # A foreign edit must survive restoration and a subsequent apply must still
    # use the exact original baseline, rather than adopting the temporary edit.
    $OriginalHash = New-Fixture 'conflicting-edit' $Original $true
    [void](Apply-VrVideoProfile)
    $ForeignBytes = [Text.UTF8Encoding]::new($true).GetPreamble() + [Text.Encoding]::UTF8.GetBytes("Resolution(1600,900)`r`nFSAA(2)`r`n// intervening edit`r`n")
    [IO.File]::WriteAllBytes($VideoPath, $ForeignBytes)
    $ForeignHash = (Get-FileHash $VideoPath).Hash
    # A destination read lock prevents exclusive ownership before preservation.
    $RestoreLock = [IO.File]::Open($VideoPath, 'Open', 'Read', 'Read')
    try {
        Assert-Rejected { Restore-VrVideoProfile } 'Locked conflicting settings were overwritten.'
        $Recovery = Get-Content $VideoProfileStatePath -Raw | ConvertFrom-Json
        Assert-True ($null -eq $Recovery.conflicts -and @(Get-ChildItem -LiteralPath $LocalStateDirectory -Filter 'Video.scr.conflict-*').Count -eq 0) 'Failed destination ownership created conflict evidence.'
        Assert-True ((Get-FileHash $VideoPath).Hash -eq $ForeignHash -and (Test-Path $VideoProfileBackupPath)) 'Failed ownership lost foreign settings or baseline.'
    } finally { $RestoreLock.Dispose() }
    # Deny journal replacement after preservation to expose retryable evidence.
    $JournalLock = [IO.File]::Open($VideoProfileStatePath, 'Open', 'Read', 'Read')
    try {
        Assert-Rejected { Restore-VrVideoProfile } 'Locked conflict journal was replaced.'
        $Copies = @(Get-ChildItem -LiteralPath $LocalStateDirectory -Filter 'Video.scr.conflict-*')
        Assert-True ($Copies.Count -eq 1) 'Failed conflict journaling did not preserve foreign settings.'
        $Conflict = [pscustomobject]@{path=$Copies[0].FullName;sha256=$ForeignHash}
    } finally { $JournalLock.Dispose() }
    # Persist the actual preserved copy to exercise damaged journaled evidence.
    $Recovery | Add-Member -NotePropertyName conflicts -NotePropertyValue @($Conflict) -Force
    Write-CoJVrVideoRecoveryJournal -StatePath $VideoProfileStatePath -State $Recovery
    Assert-True ($Conflict.path -ne $VideoPath -and $Conflict.path -ne $VideoProfileBackupPath) 'Conflict copy replaced a transaction input.'
    Assert-True ($Conflict.sha256 -eq $ForeignHash -and (Get-FileHash -LiteralPath $Conflict.path).Hash -eq $ForeignHash) 'Conflict journal does not describe the preserved intervening bytes.'
    Assert-True ((Get-FileHash $VideoPath).Hash -eq $ForeignHash -and (Test-Path $VideoProfileBackupPath)) 'Failed restoration lost foreign settings or baseline.'
    Assert-True (Restore-VrVideoProfile) 'Conflict restoration could not be retried.'
    Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) 'Conflict recovery did not restore original bytes.'
    Assert-True ((Get-FileHash -LiteralPath $Conflict.path).Hash -eq $ForeignHash) 'Cleanup removed or changed the conflict copy.'
    [void](Apply-VrVideoProfile)
    Assert-True ((Get-FileHash $VideoProfileBackupPath).Hash -eq $OriginalHash) 'Reapply adopted the intervening edit as its baseline.'
    [void](Restore-VrVideoProfile)
    Assert-True ((Get-FileHash -LiteralPath $Conflict.path).Hash -eq $ForeignHash) 'Reapply cleanup deleted an older conflict copy.'

    foreach ($Damage in @('missing', 'tampered', 'partial')) {
        $OriginalHash = New-Fixture "conflict-$Damage" $Original
        [void](Apply-VrVideoProfile)
        [IO.File]::WriteAllBytes($VideoPath, $ForeignBytes)
        $JournalLock = [IO.File]::Open($VideoProfileStatePath, 'Open', 'Read', 'Read')
        try { Assert-Rejected { Restore-VrVideoProfile } 'Restoration ignored its journal lock.' }
        finally { $JournalLock.Dispose() }
        $Recovery = Get-Content $VideoProfileStatePath -Raw | ConvertFrom-Json
        $Copies = @(Get-ChildItem -LiteralPath $LocalStateDirectory -Filter 'Video.scr.conflict-*')
        Assert-True ($Copies.Count -eq 1) 'Journal interruption lost conflict evidence.'
        $Recovery | Add-Member -NotePropertyName conflicts -NotePropertyValue @([pscustomobject]@{path=$Copies[0].FullName;sha256=$ForeignHash}) -Force
        Write-CoJVrVideoRecoveryJournal -StatePath $VideoProfileStatePath -State $Recovery
        $Conflict = $Recovery.conflicts[0]
        if ($Damage -eq 'missing') { Remove-Item -LiteralPath $Conflict.path }
        elseif ($Damage -eq 'partial') { [IO.File]::WriteAllBytes($Conflict.path, $ForeignBytes[0..7]) }
        else { [IO.File]::WriteAllText($Conflict.path, 'tampered conflict') }
        Assert-Rejected { Restore-VrVideoProfile } "A $Damage journaled conflict copy was accepted."
        Assert-True ((Get-FileHash $VideoPath).Hash -eq $ForeignHash) 'Invalid conflict recovery overwrote intervening settings.'
        Assert-True ((Test-Path $VideoProfileStatePath) -and (Test-Path $VideoProfileBackupPath)) 'Invalid conflict recovery discarded retry evidence.'
        if ($Damage -ne 'missing') {
            Assert-True ([IO.File]::ReadAllBytes($Conflict.path).Length -ne $ForeignBytes.Length) 'Invalid conflict copy was silently replaced.'
        }
        # Only explicit repair of the retained copy permits the retry.
        [IO.File]::WriteAllBytes($Conflict.path, $ForeignBytes)
        Assert-True (Restore-VrVideoProfile) 'Repaired conflict evidence could not be retried.'
        Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash -and (Test-Path -LiteralPath $Conflict.path)) 'Repaired recovery lost original or conflict bytes.'
    }

    # At both pre-write conflict journaling and post-verification commit, a
    # competing handle must be denied write/delete access. No timing or sleeps.
    $RealJournalWriter = (Get-Command Write-CoJVrVideoRecoveryJournal).ScriptBlock
    try {
        function Write-CoJVrVideoRecoveryJournal {
            param([string]$StatePath, [object]$State)
            $script:WriterProbeCount++
            Assert-Rejected {
                $Writer = [IO.File]::Open($State.videoPath, 'Open', 'Write', 'ReadWrite')
                $Writer.Dispose()
            } 'Concurrent writer acquired Video.scr during recovery.'
            Assert-Rejected { [IO.File]::Delete($State.videoPath) } 'Concurrent delete removed Video.scr during recovery.'
            & $RealJournalWriter -StatePath $StatePath -State $State
        }
        foreach ($Case in @('applied', 'foreign', 'original', 'missing')) {
            $OriginalHash = New-Fixture "exclusive-$Case" $Original $true
            [void](Apply-VrVideoProfile)
            if ($Case -eq 'foreign') { [IO.File]::WriteAllBytes($VideoPath, $ForeignBytes) }
            if ($Case -eq 'original') { Copy-Item -LiteralPath $VideoProfileBackupPath -Destination $VideoPath -Force }
            if ($Case -eq 'missing') { Remove-Item -LiteralPath $VideoPath }
            $script:WriterProbeCount = 0
            Assert-True (Restore-VrVideoProfile) 'Exclusive recovery failed.'
            $ExpectedProbes = if ($Case -eq 'foreign') { 2 } else { 1 }
            Assert-True ($WriterProbeCount -eq $ExpectedProbes) 'Recovery skipped ownership checks at journaling boundaries.'
            Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) 'Exclusive recovery did not restore exact bytes.'
            $Writer = [IO.File]::Open($VideoPath, 'Open', 'Write', 'None')
            $Writer.Dispose()
        }
    } finally { Set-Item Function:Write-CoJVrVideoRecoveryJournal -Value $RealJournalWriter }

    foreach ($BackupCondition in @('valid', 'missing', 'corrupt')) {
        $OriginalHash = New-Fixture "already-original-$BackupCondition" $Original $true
        [void](Apply-VrVideoProfile)
        Copy-Item -LiteralPath $VideoProfileBackupPath -Destination $VideoPath -Force
        if ($BackupCondition -eq 'missing') { Remove-Item -LiteralPath $VideoProfileBackupPath }
        if ($BackupCondition -eq 'corrupt') { [IO.File]::WriteAllText($VideoProfileBackupPath, 'unneeded corrupt backup') }
        $RestoreLock = [IO.File]::Open($VideoPath, 'Open', 'Read', 'Read')
        try { Assert-True (Restore-VrVideoProfile) 'Already-original recovery attempted an unnecessary write or required an unused backup.' }
        finally { $RestoreLock.Dispose() }
        Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) 'Already-original recovery changed bytes.'
    }

    $OriginalHash = New-Fixture 'conflict-journal-interruption' $Original
    [void](Apply-VrVideoProfile)
    [IO.File]::WriteAllBytes($VideoPath, $ForeignBytes)
    $JournalHash = (Get-FileHash $VideoProfileStatePath).Hash
    $JournalLock = [IO.File]::Open($VideoProfileStatePath, 'Open', 'Read', 'Read')
    try {
        Assert-Rejected { Restore-VrVideoProfile } 'Conflict journal update ignored its write lock.'
        Assert-True ((Get-FileHash $VideoProfileStatePath).Hash -eq $JournalHash -and (Get-FileHash $VideoPath).Hash -eq $ForeignHash) 'Failed conflict journal update damaged recovery or overwrote intervening bytes.'
        $InterruptedCopies = @(Get-ChildItem -LiteralPath $LocalStateDirectory -Filter 'Video.scr.conflict-*')
        Assert-True ($InterruptedCopies.Count -eq 1 -and (Get-FileHash -LiteralPath $InterruptedCopies[0].FullName).Hash -eq $ForeignHash) 'Interrupted conflict preservation did not retain its copy.'
    } finally { $JournalLock.Dispose() }
    Assert-True (Restore-VrVideoProfile) 'Interrupted conflict journaling could not be retried.'
    Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash -and (Test-Path -LiteralPath $InterruptedCopies[0].FullName)) 'Conflict retry lost original bytes or the interrupted copy.'

    foreach ($Case in @(
        @{ Name='1080p'; Resolution='Resolution(1920,1080)'; Expected='1920x1080' },
        @{ Name='wide'; Resolution="  Resolution( 3440, 1440 ) // selected"; Expected='3440x1440' }
    )) {
        $OriginalHash = New-Fixture $Case.Name ($Case.Resolution + "`nFSAA(0)`nTextureQuality(`"Normal`")`n")
        $Profile = Apply-VrVideoProfile
        Assert-True ($Profile.resolution -eq $Case.Expected) 'Selected resolution was not recorded.'
        Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) 'Already-compatible file was unnecessarily rewritten.'
        [void](Restore-VrVideoProfile)
    }

    $NativeOriginal = "!Fullscreen()`r`nResolution(2560,1440) // original`r`nFSAA(4)`r`nFullscreen() // original mode`r`nTextureQuality(`"Normal`")`r`n"
    $OriginalHash = New-Fixture 'native-2804' $NativeOriginal $true
    $Profile = Apply-VrVideoProfile -NativeResolution '2804x2860'
    $Applied = [IO.File]::ReadAllText($VideoPath)
    Assert-True ($Applied -match '(?m)^Resolution\(2804,2860\) // original\r?$') 'Requested native resolution was not applied.'
    Assert-True ($Applied -notmatch '(?m)^Fullscreen\(') 'Native high-resolution profile must use windowed presentation.'
    Assert-True ($Applied.Contains('!Fullscreen()') -and $Applied.Contains('TextureQuality("Normal")')) 'Native profile changed declarations or quality.'
    Assert-True ($Profile.resolution -eq '2804x2860' -and $Profile.originalResolution -eq '2560x1440' -and $Profile.windowed) 'Native profile provenance is incomplete.'
    Assert-True ((Get-FileHash $VideoPath).Hash -eq $Profile.appliedSha256) 'Native profile hash does not match settings.'
    [void](Restore-VrVideoProfile)
    Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) 'Native profile did not restore original resolution, fullscreen and exact bytes.'

    $OriginalHash = New-Fixture 'native-already-windowed' "Resolution(1280,720)`nFSAA(0)`n"
    $Profile = Apply-VrVideoProfile -NativeResolution '2804x2860'
    Assert-True ($Profile.windowed -and $Profile.resolution -eq '2804x2860') 'Already-windowed native profile failed.'
    [void](Restore-VrVideoProfile)
    Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) 'Windowed native profile lost original settings.'

    foreach ($Extent in @(
        @{ Width=1920; Height=1200; Expected='1920x1200' },
        @{ Width=2560; Height=1440; Expected='2560x1440' },
        # This boundary tests only native integer representation, not engine,
        # GPU or memory admission. The parent owns physical candidate preflight.
        @{ Width=[int]::MaxValue; Height=1; Expected='2147483647x1' }
    )) {
        $OriginalHash = New-Fixture ('experimental-' + $Extent.Expected) $NativeOriginal $true
        $Profile = Apply-CoJVrVideoProfile -VideoPath $VideoPath -StatePath $VideoProfileStatePath -BackupPath $VideoProfileBackupPath -ExperimentalWidth $Extent.Width -ExperimentalHeight $Extent.Height
        $Expected = $NativeOriginal.Replace('Resolution(2560,1440)', "Resolution($($Extent.Width),$($Extent.Height))").Replace('FSAA(4)', 'FSAA(0)').Replace("`nFullscreen()", "`n// VR native-resolution trial: Fullscreen()")
        Assert-True ([IO.File]::ReadAllText($VideoPath) -eq $Expected) 'Experimental startup changed unrelated settings or silently clamped an axis.'
        Assert-True ($Profile.experimental -eq $true -and $Profile.requestedWidth -eq $Extent.Width -and $Profile.requestedHeight -eq $Extent.Height -and $Profile.resolution -eq $Extent.Expected -and $Profile.nativeResolution -eq $Extent.Expected -and $Profile.windowed) 'Experimental requested/applied provenance is incomplete.'
        $Journal = Get-Content $VideoProfileStatePath -Raw | ConvertFrom-Json
        Assert-True ($Journal.experimental -eq $true -and $Journal.requestedWidth -eq $Extent.Width -and $Journal.requestedHeight -eq $Extent.Height -and (Get-FileHash $VideoPath).Hash -eq $Journal.appliedSha256) 'Experimental journal does not describe actual bytes and requested extent.'
        [void](Restore-VrVideoProfile)
        Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) 'Experimental profile did not restore exact original bytes.'
    }

    foreach ($Arguments in @(
        @{ ExperimentalWidth=1920 },
        @{ ExperimentalHeight=1080 },
        @{ ExperimentalWidth=0; ExperimentalHeight=1080 },
        @{ ExperimentalWidth=1920; ExperimentalHeight=0 },
        @{ ExperimentalWidth=-1; ExperimentalHeight=1080 },
        @{ ExperimentalWidth=1920; ExperimentalHeight=-1 },
        @{ ExperimentalWidth=([long][int]::MaxValue + 1); ExperimentalHeight=1080 },
        @{ ExperimentalWidth=1920; ExperimentalHeight=([long][int]::MaxValue + 1) },
        @{ ExperimentalWidth='999999999999999999999999'; ExperimentalHeight=1080 },
        @{ ExperimentalWidth=1920.5; ExperimentalHeight=1080 },
        @{ ExperimentalWidth=$true; ExperimentalHeight=1080 },
        @{ ExperimentalWidth=1920; ExperimentalHeight='invalid' },
        @{ ExperimentalWidth=1920; ExperimentalHeight=1080; NativeResolution='2804x2860' },
        @{ NativeResolution='1920x1080' }
    )) {
        $OriginalHash = New-Fixture ('invalid-request-' + [guid]::NewGuid().ToString('N')) $NativeOriginal $true
        Assert-Rejected { Apply-CoJVrVideoProfile -VideoPath $VideoPath -StatePath $VideoProfileStatePath -BackupPath $VideoProfileBackupPath @Arguments } 'Invalid experimental dimensions or a conflicting fixed override were accepted.'
        Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) 'Rejected dimension request modified original settings.'
        Assert-True (-not (Test-Path $VideoProfileBackupPath) -and -not (Test-Path $VideoProfileStatePath)) 'Rejected dimensions created a transaction.'
    }

    foreach ($Fullscreen in @("Fullscreen(bad)", "Fullscreen()`nFullscreen()")) {
        $OriginalHash = New-Fixture ('ambiguous-mode-' + [guid]::NewGuid().ToString('N')) ("Resolution(2560,1440)`nFSAA(4)`n" + $Fullscreen)
        Assert-Rejected { Apply-VrVideoProfile -NativeResolution '2804x2860' } 'Ambiguous fullscreen override was accepted.'
        Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) 'Rejected fullscreen override modified settings.'
        Assert-True (-not (Test-Path $VideoProfileBackupPath) -and -not (Test-Path $VideoProfileStatePath)) 'Rejected fullscreen override created recovery state.'
    }

    $Malformed = @(
        "FSAA(0)`n",
        "Resolution(2560,1440)`n",
        "Resolution(2560,1440)`nResolution(1920,1080)`nFSAA(0)`n",
        "Resolution(2560,1440)`nFSAA(0)`nFSAA(2)`n",
        "Resolution(0,1440)`nFSAA(0)`n",
        "Resolution(-1,1440)`nFSAA(0)`n",
        "Resolution(999999999999,1440)`nFSAA(0)`n",
        "Resolution(2560,`n1440)`nFSAA(0)`n",
        "Resolution(2560,1440)`nFSAA(-1)`n",
        "Resolution(2560,1440)`nFSAA(999999999999)`n",
        "Resolution(2560,1440)`nFSAA(0)`nFSAA(bad)`n",
        "Resolution(2560,1440)`nResolution(bad)`nFSAA(0)`n"
    )
    for ($Index = 0; $Index -lt $Malformed.Count; ++$Index) {
        $OriginalHash = New-Fixture "malformed-$Index" $Malformed[$Index]
        Assert-Rejected { Apply-VrVideoProfile } "Malformed fixture $Index was accepted."
        Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) "Rejected fixture $Index was modified."
        Assert-True (-not (Test-Path $VideoProfileBackupPath) -and -not (Test-Path $VideoProfileStatePath)) 'Malformed input created a transaction.'
    }

    $OriginalHash = New-Fixture 'corrupt-backup' "Resolution(2560,1440)`nFSAA(2)`n"
    [void](Apply-VrVideoProfile)
    $AppliedHash = (Get-FileHash $VideoPath).Hash
    [IO.File]::WriteAllText($VideoProfileBackupPath, 'corrupt')
    Assert-Rejected { Restore-VrVideoProfile } 'A corrupt backup was restored.'
    Assert-True ((Get-FileHash $VideoPath).Hash -eq $AppliedHash) 'Corrupt backup rejection altered the config.'
    Assert-True (Test-Path $VideoProfileStatePath) 'Corrupt backup rejection discarded recovery evidence.'

    [void](New-Fixture 'orphan-backup' "Resolution(2560,1440)`nFSAA(2)`n")
    [IO.File]::WriteAllText($VideoProfileBackupPath, 'original recovery evidence')
    Assert-Rejected { Apply-VrVideoProfile } 'Apply overwrote an orphaned backup.'
    Assert-True ([IO.File]::ReadAllText($VideoProfileBackupPath) -eq 'original recovery evidence') 'Orphaned backup was overwritten.'

    $OriginalHash = New-Fixture 'journal-write-failure' "Resolution(2560,1440)`nFSAA(2)`n"
    $VideoProfileStatePath = Join-Path $LocalStateDirectory 'missing-parent/profile.json'
    Assert-Rejected { Apply-VrVideoProfile } 'Journal write failure was ignored.'
    Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) 'Journal failure left modified settings.'
    Assert-True (-not (Test-Path $VideoProfileBackupPath) -and -not (Test-Path $VideoProfileStatePath)) 'Journal failure left a spurious transaction.'

    $OriginalHash = New-Fixture 'config-write-failure' "Resolution(2560,1440)`nFSAA(2)`n"
    $LockedFile = [IO.File]::Open($VideoPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        Assert-Rejected { Apply-VrVideoProfile } 'Config write failure was ignored.'
        Assert-True (-not (Test-Path $VideoProfileStatePath) -and -not (Test-Path $VideoProfileBackupPath)) 'Unchanged original was not safely cleaned up after apply failed.'
    } finally { $LockedFile.Dispose() }
    Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) 'Config write failure corrupted the original settings.'
    Assert-True (-not (Restore-VrVideoProfile)) 'Completed rollback left an active transaction.'

    [void](New-Fixture 'missing-backup' "Resolution(2560,1440)`nFSAA(2)`n")
    [void](Apply-VrVideoProfile)
    $AppliedHash = (Get-FileHash $VideoPath).Hash
    Remove-Item -LiteralPath $VideoProfileBackupPath
    Assert-Rejected { Restore-VrVideoProfile } 'Missing backup was accepted.'
    Assert-True ((Get-FileHash $VideoPath).Hash -eq $AppliedHash -and (Test-Path $VideoProfileStatePath)) 'Missing backup rejection changed settings or discarded evidence.'

    [void](New-Fixture 'invalid-recovery-journal' "Resolution(2560,1440)`nFSAA(2)`n")
    [void](Apply-VrVideoProfile)
    $AppliedHash = (Get-FileHash $VideoPath).Hash
    [IO.File]::WriteAllText($VideoProfileStatePath, '{"schemaVersion":1,"videoPath":"","originalSha256":""}')
    Assert-Rejected { Restore-VrVideoProfile } 'Invalid recovery journal was accepted.'
    Assert-True ((Get-FileHash $VideoPath).Hash -eq $AppliedHash -and (Test-Path $VideoProfileBackupPath)) 'Invalid journal rejection changed settings or discarded recovery backup.'

    foreach ($CleanupTarget in @('backup','journal')) {
        $OriginalHash = New-Fixture "cleanup-$CleanupTarget" "Resolution(2560,1440)`nFSAA(2)`n"
        [void](Apply-VrVideoProfile)
        $LockedPath = if ($CleanupTarget -eq 'backup') { $VideoProfileBackupPath } else { $VideoProfileStatePath }
        # Permit reads/writes, deny deletion: restoration succeeds but cleanup fails.
        $CleanupLock = [IO.File]::Open($LockedPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
        try {
            Assert-Rejected { Restore-VrVideoProfile } 'Cleanup lock was ignored.'
            Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) 'Cleanup failure lost restored settings.'
            Assert-True (Test-Path $VideoProfileStatePath) 'Cleanup failure lost its retry journal.'
        } finally { $CleanupLock.Dispose() }
        Assert-True (Restore-VrVideoProfile) 'Interrupted cleanup could not be retried.'
        Assert-True (-not (Test-Path $VideoProfileBackupPath) -and -not (Test-Path $VideoProfileStatePath)) 'Cleanup retry left an orphan.'
    }

    Write-Host 'VR video profile preservation, validation and transactional recovery tests passed.'
} finally {
    # This resolved path is created under the caller's test directory only.
    if (-not $TestRoot.StartsWith([IO.Path]::GetFullPath($BinaryDirectory) + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Test cleanup escaped BinaryDirectory.'
    }
    Remove-Item -LiteralPath $TestRoot -Recurse -Force
}
