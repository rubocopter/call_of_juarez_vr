param([string]$SourceDirectory=(Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference='Stop'
. (Join-Path $SourceDirectory 'release/release_common.ps1')
. (Join-Path $SourceDirectory 'tools/coj_video_profile.ps1')
$Ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $SourceDirectory 'release/cojvr.ps1'),[ref]$null,[ref]$null)
$Definition=$Ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Update-PublicSessionVideo'},$true)
if(-not $Definition){throw 'Missing session video transaction boundary'}
Invoke-Expression $Definition.Extent.Text
foreach($Name in @('Start-PublicSession','Restore-PublicInstall','Assert-PublicVideoRecoveryPaths')) {
    $Definition=$Ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $Name},$true)
    Invoke-Expression $Definition.Extent.Text
}
# Exercise the real reporting functions when present; the pre-fix transaction
# must fail the output assertions below, rather than fail on a missing helper.
foreach($Definition in $Ast.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and
    $n.Name -in @('Get-PublicResolutionFallbackReason','Show-PublicResolutionOutcome')},$true)) {
    Invoke-Expression $Definition.Extent.Text
}
function Assert-PublicStageIdentity { }
function Read-PublicResolutionSettings { [pscustomobject]@{profile=$script:Profile;width=$script:Width;height=$script:Height} }
function Resolve-PublicResolution {
    param($Settings)
    if($script:Reject){throw 'injected invalid proposal'}
    if($null -ne $script:DecisionOverride){return $script:DecisionOverride}
    [pscustomobject]@{Profile=$Settings.profile;Status=$(if($Settings.width -ne 0){'experimental_trial'}else{'preserve'});
        Width=$Settings.width;Height=$Settings.height;Experimental=($Settings.width -ne 0);Proposal=$null;Limits=$null}
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
    $script:Reject=$false;$script:Profile='Auto';$script:Width=0;$script:Height=0
    $Secret='C:\Users\private-user\secret-runtime.dll'
    # A missing preference resolves to Auto in the settings tests. Here the real
    # transaction must explain fallback without echoing subprocess exceptions.
    $Fallbacks=@(
        @('preserve_unavailable','helper_recommendation_unavailable','hmd_not_present','', 'headset.*not.*detected'),
        @('preserve_unavailable','helper_recommendation_unavailable','background_init_failed','', 'SteamVR.*unavailable'),
        @('preserve_unavailable',('query_helper_unavailable: '+$Secret),'','', 'query helper.*unavailable'),
        @('preserve_unavailable',('resolver_unavailable: '+$Secret),'','', 'calculator.*unavailable'),
        @('preserve_unavailable','','',$Secret, 'native.*limits.*unavailable'),
        @('preserve_incompatible','','','texture_axis_limit_exceeded', 'texture.*dimension.*limit'),
        @('preserve_incompatible','','','menu_source_128mib_limit_exceeded', 'menu.*size.*limit'),
        @('preserve_adapter_unqualified','','','', 'adapter.*not.*qualified'),
        @('preserve_unavailable',$Secret,$Secret,'', 'resolution.*unavailable')
    )
    foreach($Case in $Fallbacks){
        $script:DecisionOverride=[pscustomobject]@{Profile='Auto';Status=$Case[0];Width=0;Height=0;Experimental=$false;
            Proposal=[pscustomobject]@{reason=$Case[1];observation=[pscustomobject]@{error=$Case[2];vrInitErrorSymbol=$Secret}};
            Limits=$(if($Case[3]){[pscustomobject]@{error=$Case[3]}}else{$null})}
        $Output=(Update-PublicSessionVideo $Root $Run 6>&1 | Out-String)
        Assert-True ($Output -match $Case[4]) "Fallback cause was not explained safely: $($Case[0]) / $($Case[4])"
        Assert-True (-not $Output.Contains($Secret) -and $Output.Length -lt 1400) 'Fallback leaked arbitrary helper text or unbounded output'
        Assert-True ($Output -match 'configured startup request 1280x720' -and $Output -match 'raster.*not observed') 'Fallback confused configuration with native raster'
        Assert-True ($Output -match 'Prepare session') 'Fallback omitted the explicit retry route'
        Assert-True ((Get-FileHash $Video).Hash -eq $Baseline.appliedSha256) 'Fallback changed selected resolution/fullscreen'
        Assert-True ($Run.validation.videoProfile.resolutionProfile -eq 'Auto' -and
            -not $Run.validation.videoProfile.experimentalResolution) 'Fallback lost Auto intent'
    }
    $script:DecisionOverride=[pscustomobject]@{Profile='Auto';Status='experimental_trial';Width=2804;Height=2860;
        Experimental=$true;Proposal=$null;Limits=$null}
    $Output=(Update-PublicSessionVideo $Root $Run 6>&1 | Out-String)
    Assert-True ($Output -match 'configured startup request 2804x2860' -and $Output -match 'raster.*not observed') 'Successful Auto claimed native execution or omitted configured extent'
    Assert-True ((Get-FileHash (Join-Path $Root '.cojvr-session-video.backup')).Hash -eq $Baseline.appliedSha256) 'Auto retry replaced the original baseline'
    $script:DecisionOverride=$null
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
