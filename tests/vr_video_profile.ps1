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
    $Original = "!Resolution(i,i)`r`n!FSAA(i) // declaration`r`nResolution(2560,1440)`r`nFSAA(4)`r`nTextureQuality(`"Normal`")`r`nFiltering(`"AnisotropicTrilinear`")`r`nPostprocess(`"Normal`")`r`n"
    $OriginalHash = New-Fixture 'selected-1440p' $Original $true
    $Profile = Apply-VrVideoProfile
    $Applied = [IO.File]::ReadAllText($VideoPath)
    Assert-True ($Applied -eq $Original.Replace('FSAA(4)', 'FSAA(0)')) 'Profile changed selected resolution or unrelated quality settings.'
    Assert-True ($Profile.resolution -eq '2560x1440' -and $Profile.fsaa -eq 0) 'Profile provenance does not describe the actual settings.'
    Assert-True ((Get-FileHash $VideoProfileBackupPath).Hash -eq $OriginalHash) 'Backup did not preserve original bytes.'
    Assert-True ((Get-FileHash $VideoPath).Hash -eq $Profile.appliedSha256) 'Applied provenance hash is incorrect.'
    Assert-Rejected { Apply-VrVideoProfile } 'A second apply overwrote an active transaction.'
    Assert-True (Restore-VrVideoProfile) 'Restore did not report an active transaction.'
    Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) 'Restore did not recover the original bytes/BOM/CRLF.'
    Assert-True (-not (Test-Path $VideoProfileBackupPath) -and -not (Test-Path $VideoProfileStatePath)) 'Successful restore left an active journal.'
    Assert-True (-not (Restore-VrVideoProfile)) 'Inactive restore should be a no-op.'

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
        Assert-True ((Test-Path $VideoProfileStatePath) -and (Test-Path $VideoProfileBackupPath)) 'Rollback failure discarded its journal or backup.'
    } finally { $LockedFile.Dispose() }
    Assert-True ((Get-FileHash $VideoPath).Hash -eq $OriginalHash) 'Config write failure corrupted the original settings.'
    Assert-True (Restore-VrVideoProfile) 'Rollback could not be retried after the write lock was released.'

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
