param([string]$SourceDirectory=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
. (Join-Path $SourceDirectory 'release/release_common.ps1')
. (Join-Path $SourceDirectory 'tools/coj_video_profile.ps1')
function Assert-True($Value,$Message) { if (-not $Value) { throw $Message } }
function Assert-Rejected([scriptblock]$Action,$Message) {
    $Rejected=$false
    try { & $Action } catch { $Rejected=$true }
    Assert-True $Rejected $Message
}
$Root=Join-Path $SourceDirectory ('work/release-paths-'+[guid]::NewGuid().ToString('N'))
$Links=@()
New-Item -ItemType Directory -Path $Root -Force | Out-Null
try {
    # Known-folder redirection changes the path; its spelling is not authority.
    $Documents=Join-Path $Root 'OneDrive - Test/Documents'
    $Settings=Join-Path $Documents 'call of juarez/out/Settings'
    New-Item -ItemType Directory -Path $Settings -Force | Out-Null
    $Video=Join-Path $Settings 'Video.scr'
    $Original="Resolution(1280,720)`r`nFSAA(4)`r`n"
    [IO.File]::WriteAllText($Video,$Original)
    Assert-CojvrNoReparse $Video
    $Outside=Join-Path $Root 'outside'
    New-Item -ItemType Directory -Path $Outside | Out-Null
    $Link=Join-Path $Settings 'linked'
    [void](New-Item -ItemType Junction -Path $Link -Target $Outside)
    $Links += $Link
    [IO.File]::WriteAllText((Join-Path $Outside 'existing.scr'),'private')
    Assert-Rejected { Assert-CojvrNoReparse (Join-Path $Link 'existing.scr') } 'Existing destination through junction admitted.'
    Assert-Rejected { Assert-CojvrNoReparse (Join-Path $Link 'missing/Video.scr') } 'Missing destination hid a junction ancestor.'
    # Link-like names alone do not grant the cloud exception.
    Assert-Rejected { Assert-CojvrNoReparse (Join-Path $Link 'existing.scr') -AllowCloudFiles } 'OneDrive-named path admitted a real junction.'

    # Cloud provider metadata is the only simulated boundary: ordinary sandbox
    # files still exercise real backup, mutation, failure and restoration I/O.
    $RealMetadata=${function:Get-CojvrPathMetadata}
    $script:CloudTag=[uint32]2415919130 # IO_REPARSE_TAG_CLOUD
    function Get-CojvrPathMetadata([string]$Path) {
        $Info=& $RealMetadata $Path
        if ($Path -eq $Documents -or $Path -eq $Video) {
            return [pscustomobject]@{Attributes=($Info.Attributes -bor 1024);ReparseTag=$script:CloudTag}
        }
        return $Info
    }
    foreach ($Tag in @([uint32]2415919130,[uint32]2415923226,[uint32]2415980570)) {
        $script:CloudTag=$Tag
        Assert-CojvrNoReparse $Video -AllowCloudFiles
        Assert-Rejected { Assert-CojvrNoReparse $Video } 'Cloud exception escaped video-only opt-in.'
    }
    foreach ($Tag in @([uint32]2684354572,[uint32]2684354563,[uint32]2415919132,[uint32]2952790042,[uint32]0)) {
        $script:CloudTag=$Tag
        Assert-Rejected { Assert-CojvrNoReparse $Video -AllowCloudFiles } 'Link, unknown or unreadable reparse tag admitted as cloud.'
    }
    $script:CloudTag=[uint32]2415919130
    Assert-Rejected { Assert-CojvrNoReparse (Join-Path $Link 'existing.scr') -AllowCloudFiles } 'Cloud opt-in admitted a junction.'
    # An unreadable metadata query must not be treated as a missing path.
    $CloudMetadata=${function:Get-CojvrPathMetadata}
    function Get-CojvrPathMetadata([string]$Path) { throw 'injected metadata access denied' }
    Assert-Rejected { Assert-CojvrNoReparse $Video -AllowCloudFiles } 'Unreadable metadata admitted.'
    Set-Item -LiteralPath Function:\Get-CojvrPathMetadata -Value $CloudMetadata
    $State=Join-Path $Root 'video.json';$Backup=Join-Path $Root 'video.backup'
    Assert-CojvrNoReparse $Video -AllowCloudFiles
    [void](Apply-CoJVrVideoProfile -VideoPath $Video -StatePath $State -BackupPath $Backup)
    Assert-True ([IO.File]::ReadAllText($Video) -match 'FSAA\(0\)') 'Redirected video was not updated.'
    [void](Restore-CoJVrVideoProfile -StatePath $State -BackupPath $Backup)
    Assert-True ([IO.File]::ReadAllText($Video) -eq $Original) 'Redirected video did not restore exact original.'

    # The launcher's recovery guard must recheck persisted destinations. Swapping
    # a directory for a junction after installation cannot redirect recovery I/O.
    $Ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $SourceDirectory 'release/cojvr.ps1'),[ref]$null,[ref]$null)
    foreach ($Name in @('Assert-PublicVideoRecoveryPaths','Restore-PublicInstall','Update-PublicSessionVideo')) {
        $Definition=$Ast.Find({param($Node) $Node -is [Management.Automation.Language.FunctionDefinitionAst] -and $Node.Name -eq $Name},$true)
        Invoke-Expression $Definition.Extent.Text
    }
    function Assert-PublicStageIdentity { }
    $PublicState=Join-Path $Root '.cojvr-public-video.json'
    $PublicBackup=Join-Path $Root '.cojvr-public-video.backup'
    [void](Apply-CoJVrVideoProfile -VideoPath $Video -StatePath $PublicState -BackupPath $PublicBackup)
    Assert-PublicVideoRecoveryPaths $Root
    $RetainedSettings=$Settings+'-retained'
    Move-Item -LiteralPath $Settings -Destination $RetainedSettings
    [void](New-Item -ItemType Junction -Path $Settings -Target $Outside)
    $Links += $Settings
    $OutsideVideo=Join-Path $Outside 'Video.scr'
    [IO.File]::WriteAllText($OutsideVideo,'unrelated destination')
    Assert-Rejected { Restore-PublicInstall $Root ([pscustomobject]@{}) } 'Recovery followed a replaced video directory.'
    Assert-Rejected { Update-PublicSessionVideo $Root ([pscustomobject]@{validation=[pscustomobject]@{}}) } 'Session preparation followed a replaced video directory.'
    Assert-True ([IO.File]::ReadAllText($OutsideVideo) -eq 'unrelated destination') 'Path rejection modified unrelated destination.'
    Assert-True ((Test-Path -LiteralPath $PublicState) -and (Test-Path -LiteralPath $PublicBackup)) 'Path rejection discarded recovery evidence.'
    [IO.Directory]::Delete($Settings)
    $Links=@($Links | Where-Object {$_ -ne $Settings})
    Move-Item -LiteralPath $RetainedSettings -Destination $Settings
    [void](Restore-CoJVrVideoProfile -StatePath $PublicState -BackupPath $PublicBackup)
    Assert-True ([IO.File]::ReadAllText($Video) -eq $Original) 'Recovery retry lost redirected original.'
    # Fail after journaling, during destination publication. Recovery must retain
    # the original, remove its completed recovery layer, and report failure.
    function Get-FileHash {
        param([string]$LiteralPath,[string]$Algorithm)
        if ($LiteralPath -eq $Video) {throw 'injected video publication verification failure'}
        Microsoft.PowerShell.Utility\Get-FileHash @PSBoundParameters
    }
    Assert-Rejected { Apply-CoJVrVideoProfile -VideoPath $Video -StatePath $State -BackupPath $Backup } 'Injected video failure passed.'
    Assert-True ([IO.File]::ReadAllText($Video) -eq $Original) 'Redirected video apply failure lost original bytes.'
    Assert-True (-not (Test-Path -LiteralPath $State) -and -not (Test-Path -LiteralPath $Backup)) 'Completed rollback left an active recovery layer.'
    Remove-Item Function:\Get-FileHash
    Write-Host 'PASS redirected Documents, cloud metadata allowlist, real junction and missing ancestor rejection, exact restore and failed publication rollback.'
} finally {
    # Remove junction objects without recursing through their targets.
    foreach ($Path in $Links) {
        if (-not ([IO.Path]::GetFullPath($Path)).StartsWith([IO.Path]::GetFullPath($Root)+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Unsafe link cleanup'}
        [IO.Directory]::Delete($Path)
    }
    if (-not ([IO.Path]::GetFullPath($Root)).StartsWith([IO.Path]::GetFullPath((Join-Path $SourceDirectory 'work'))+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Unsafe fixture cleanup'}
    Remove-Item -LiteralPath $Root -Recurse -Force
}
