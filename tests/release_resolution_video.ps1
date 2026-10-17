param([string]$SourceDirectory=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
. (Join-Path $SourceDirectory 'release/release_common.ps1')
. (Join-Path $SourceDirectory 'tools/coj_video_profile.ps1')
$Ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $SourceDirectory 'release/cojvr.ps1'),[ref]$null,[ref]$null)
$Definition=$Ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Update-PublicSessionVideo'},$true)
if(-not $Definition){throw 'Missing session video transaction boundary'}
Invoke-Expression $Definition.Extent.Text
foreach($Name in @('Start-PublicSession','Restore-PublicInstall')) {
    $Definition=$Ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $Name},$true)
    Invoke-Expression $Definition.Extent.Text
}
function Assert-PublicStageIdentity { }
function Read-PublicResolutionSettings { [pscustomobject]@{profile=$script:Profile;width=$script:Width;height=$script:Height} }
function Resolve-PublicResolution {
    param($Settings)
    if($script:Reject){throw 'injected invalid proposal'}
    [pscustomobject]@{Profile=$Settings.profile;Status='fixture';Width=$Settings.width;Height=$Settings.height;Experimental=($Settings.width -ne 0)}
}
function Assert-PublicNativeResolutionContext { }
function Assert-True($Value,$Message){if(-not $Value){throw $Message}}
$Root=Join-Path $SourceDirectory ('work/resolution-video-'+[char]0x00E9+'-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $Root -Force | Out-Null
try {
    $Video=Join-Path $Root 'Video.scr'
    [IO.File]::WriteAllText($Video,"Resolution(1280,720)`r`nFSAA(4)`r`nFullscreen()`r`n")
    $Original=(Get-FileHash $Video).Hash
    $Baseline=Apply-CoJVrVideoProfile -VideoPath $Video -StatePath (Join-Path $Root '.cojvr-public-video.json') -BackupPath (Join-Path $Root '.cojvr-public-video.backup')
    $Run=[pscustomobject]@{validation=[pscustomobject]@{}}
    $script:Reject=$false;$script:Profile='Manual';$script:Width=2804;$script:Height=2860
    Update-PublicSessionVideo $Root $Run
    Assert-True ($Run.validation.videoProfile.resolution -eq '2804x2860') 'Manual profile not applied'
    Assert-True ((Get-FileHash (Join-Path $Root '.cojvr-session-video.backup')).Hash -eq $Baseline.appliedSha256) 'Session captured overridden baseline'
    $First=(Get-FileHash $Video).Hash
    $script:Reject=$true
    try {Update-PublicSessionVideo $Root $Run;throw 'invalid proposal admitted'}catch{if($_.Exception.Message -ne 'injected invalid proposal'){throw}}
    Assert-True ((Get-FileHash $Video).Hash -eq $First) 'Invalid proposal mutated video'
    $script:Reject=$false;$script:Profile='75%';$script:Width=2428;$script:Height=2477
    [IO.File]::AppendAllText($Video,"// intervening game edit`r`n")
    $Foreign=(Get-FileHash $Video).Hash
    Update-PublicSessionVideo $Root $Run
    Assert-True ($Run.validation.videoProfile.resolution -eq '2428x2477') 'Second session retained first extent'
    Assert-True ((Get-FileHash (Join-Path $Root '.cojvr-session-video.backup')).Hash -eq $Baseline.appliedSha256) 'Second session nested overrides'
    $Copies=@(Get-ChildItem $Root -Filter 'Video.scr.conflict-*')
    Assert-True ($Copies.Count -eq 1 -and (Get-FileHash $Copies[0].FullName).Hash -eq $Foreign) 'Intervening edit lost'
    Assert-True ((Get-FileHash (Join-Path $Root '.cojvr-public-video.backup')).Hash -eq $Original) 'Install original replaced'
    $script:Profile='Preserve';$script:Width=0;$script:Height=0
    Update-PublicSessionVideo $Root $Run
    Assert-True ((Get-FileHash $Video).Hash -eq $Baseline.appliedSha256) 'Preserve failed to restore baseline dimensions/fullscreen'
    [void](Restore-CoJVrVideoProfile -StatePath (Join-Path $Root '.cojvr-session-video.json') -BackupPath (Join-Path $Root '.cojvr-session-video.backup'))
    [void](Restore-CoJVrVideoProfile -StatePath (Join-Path $Root '.cojvr-public-video.json') -BackupPath (Join-Path $Root '.cojvr-public-video.backup'))
    Assert-True ((Get-FileHash $Video).Hash -eq $Original) 'Layered uninstall lost original bytes'
    # Interrupt metadata publication after the new profile was applied. The
    # pending identity must remain recoverable, rather than permitting a new run.
    $OldDirectory=Join-Path $Root '.cojvr-evidence/runs/old-run'
    New-Item -ItemType Directory -Path $OldDirectory -Force | Out-Null
    $BuildPath=Join-Path $OldDirectory 'build-manifest.json'
    [IO.File]::WriteAllText($BuildPath,'fixture build')
    $StagePath=Join-Path $Root '.cojvr-d3d9-stage.json'
    $RunPath=Join-Path $Root '.cojvr-run.json'
    $StatePath=Join-Path $Root '.cojvr-public-install.json'
    $Stage=[pscustomobject]@{runId='old-run';buildManifestId='fixture';buildManifestSha256=(Get-FileHash $BuildPath).Hash}
    $Run=[pscustomobject]@{runId='old-run';buildManifestId='fixture';createdUtc='';status='staged';validation=[pscustomobject]@{}}
    $State=[pscustomobject]@{runId='old-run';buildManifestId='fixture';phase='installed'}
    Write-CojvrPublicJson $StagePath $Stage;Write-CojvrPublicJson $RunPath $Run;Write-CojvrPublicJson $StatePath $State
    [void](Apply-CoJVrVideoProfile -VideoPath $Video -StatePath (Join-Path $Root '.cojvr-public-video.json') -BackupPath (Join-Path $Root '.cojvr-public-video.backup'))
    $script:Profile='Manual';$script:Width=2804;$script:Height=2860
    $script:RealJsonWriter=${function:Write-CojvrPublicJson}
    function Write-CojvrPublicJson([string]$Path,$Value) {
        if ($Path -eq $RunPath) {throw 'injected metadata publication failure'}
        & $script:RealJsonWriter $Path $Value
    }
    $Failed=$false
    try {Start-PublicSession $Root $State}catch {$Failed=$_.Exception.Message -match 'injected metadata publication failure'}
    Assert-True $Failed 'Metadata failure was not reported'
    $Recovery=Get-Content $StatePath -Raw | ConvertFrom-Json
    Assert-True ($Recovery.pendingRunId -and $Recovery.runId -eq 'old-run') 'Failed session lost pending recovery ownership'
    Assert-True ((Get-Content $StagePath -Raw | ConvertFrom-Json).runId -eq 'old-run' -and (Get-Content $RunPath -Raw | ConvertFrom-Json).runId -eq 'old-run') 'Failed session left mismatched run/stage metadata'
    $Failed=$false
    try {Start-PublicSession $Root $Recovery}catch {$Failed=$_.Exception.Message -match 'interrupted'}
    Assert-True $Failed 'Interrupted session admitted another profile application'
    # Staging restoration is covered separately by deployment fixtures. Here the
    # real uninstall video ordering must remove both layers after interruption.
    Remove-Item -LiteralPath $StagePath
    Restore-PublicInstall $Root $Recovery
    Assert-True ((Get-FileHash $Video).Hash -eq $Original -and -not (Test-Path $StatePath)) 'Recovery lost original video or installation journal'
    Write-Host 'PASS startup video reapply, preserve, conflicts and original baseline restoration.'
} finally {
    if(-not ([IO.Path]::GetFullPath($Root)).StartsWith([IO.Path]::GetFullPath((Join-Path $SourceDirectory 'work'))+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe fixture cleanup'}
    Remove-Item -LiteralPath $Root -Recurse -Force
}
