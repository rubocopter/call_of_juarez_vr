param([string]$SourceDirectory,[string]$BinaryDirectory)
$ErrorActionPreference='Stop'
$RepositoryRoot=$SourceDirectory
. (Join-Path $SourceDirectory 'tools/coj_video_profile.ps1')
. (Join-Path $SourceDirectory 'tools/coj_resolution_session.ps1')
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $SourceDirectory 'tools/vr_test.ps1'),[ref]$null,[ref]$null)
$workflowLock=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Enter-VrWorkflowLock'},$true)
if(-not $workflowLock){throw 'Missing prepare/finish exclusion'}
Invoke-Expression $workflowLock.Extent.Text
$lockDirectory=Join-Path ([IO.Path]::GetTempPath()) ('cojvr-workflow-lock-'+[guid]::NewGuid().ToString('N'))
$firstLease=$null;$secondLease=$null
try {
    $firstLease=Enter-VrWorkflowLock $lockDirectory
    $locked=$false
    try {$secondLease=Enter-VrWorkflowLock $lockDirectory} catch {
        if($_.Exception.Message -notlike '*Another VR prepare/finish*'){throw}
        $locked=$true
    }
    if(-not $locked){throw 'Concurrent prepare/finish acquired the same workflow'}
    $firstLease.Dispose();$firstLease=$null
    $secondLease=Enter-VrWorkflowLock $lockDirectory
} finally {
    if($firstLease){$firstLease.Dispose()};if($secondLease){$secondLease.Dispose()}
    Remove-Item -LiteralPath (Join-Path $lockDirectory 'vr-test.workflow.lock') -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $lockDirectory -ErrorAction SilentlyContinue
}
$memoryPolicy=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Resolve-VrLargeAddressAware'},$true)
if(-not $memoryPolicy){throw 'Missing full-body candidate LAA default policy'}
Invoke-Expression $memoryPolicy.Extent.Text
foreach($case in @(
    @($true,$false,$false,$false,$true),
    @($true,$false,$true,$false,$false),
    @($true,$false,$true,$true,$true),
    @($false,$true,$false,$false,$false),
    @($false,$true,$true,$true,$true),
    @($false,$false,$false,$false,$false),
    @($false,$false,$true,$true,$true))) {
    $actual=Resolve-VrLargeAddressAware -BodyIkAtStart:($case[0]) -StartupOnly:($case[1]) -ExplicitChoice:($case[2]) -Requested:($case[3])
    if($actual -ne $case[4]){throw 'Preparation lost default LAA or explicit diagnostic choice'}
}
$definition=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Resolve-VrResolutionProfile'},$true)
if(-not $definition){throw 'Missing startup resolution-profile boundary'}
Invoke-Expression $definition.Extent.Text
function Invoke-VrResolutionObservation {
    [pscustomobject]@{PreparationId='fixture';Record=[pscustomobject]@{
        schemaVersion=1;recordType='cojvr-resolution-observation';status=$script:QueryStatus;
        preparationId=$script:RecordId;observedUtc=[DateTime]::UtcNow.ToString('o');
        recommendedWidth=2804;recommendedHeight=2860;runtimeVersion='fixture';hmdSerial='fixture';hmdModel='fixture';
        adapterIndex=$script:AdapterIndex;applicationKey='fixture-helper';applicationKeyStatus='observed';identityQualified=$false}}
}
function Invoke-VrNativeResolutionLimits {
    param([int]$Width,[int]$Height)
    [pscustomobject]@{status=$script:CapsStatus;requestedWidth=$Width;requestedHeight=$Height;
        allocationValidated=$false;memoryQualified=$false;theaterQualified=$false}
}
$script:CapsStatus='caps_compatible';$script:AdapterIndex=0
$script:QueryStatus='observed';$script:RecordId='fixture'
$fixtureDirectory=Join-Path ([IO.Path]::GetTempPath()) ('cojvr-pixel-profile-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixtureDirectory | Out-Null
$fixtureVideo=Join-Path $fixtureDirectory 'Video.scr'
$fixtureState=Join-Path $fixtureDirectory 'state.json'
$fixtureBackup=Join-Path $fixtureDirectory 'backup.scr'
$originalVideo="Resolution(1280,720)`r`nFSAA(4)`r`nFullscreen()`r`nTextureQuality(`"Normal`")`r`n"
foreach($case in @(@('75%',2428,2477),@('100%',2804,2860),@('125%',3135,3198),@('150%',3434,3503),@('200%',3965,4045),@('Auto',2804,2860))) {
    $result=Resolve-VrResolutionProfile -Profile $case[0] -Experimental
    if($result.Proposal.observation.observedUtc -isnot [string]) {throw 'Automatic preparation lost the UTC timestamp string'}
    $beforeCulture=[Threading.Thread]::CurrentThread.CurrentCulture
    try {
        [Threading.Thread]::CurrentThread.CurrentCulture=[Globalization.CultureInfo]::GetCultureInfo('es-ES')
        $observed=[DateTimeOffset]::Parse($result.Proposal.observation.observedUtc)
        if(([DateTimeOffset]::UtcNow-$observed).TotalSeconds -gt 120){throw 'Automatic proposal observation already expired'}
    } finally {[Threading.Thread]::CurrentThread.CurrentCulture=$beforeCulture}
    if($result.Width -ne $case[1] -or $result.Height -ne $case[2] -or -not $result.Experimental -or $result.Proposal.applyAllowed) {throw 'Experimental pixel profile mismatch'}
    # Exercise the same video boundary used by preparation, on a fixture only.
    # A correct proposal alone does not show that the startup request is applied.
    [IO.File]::WriteAllText($fixtureVideo,$originalVideo,[Text.UTF8Encoding]::new($true))
    $originalHash=(Get-FileHash -LiteralPath $fixtureVideo -Algorithm SHA256).Hash
    $applied=Apply-CoJVrVideoProfile -VideoPath $fixtureVideo -StatePath $fixtureState -BackupPath $fixtureBackup `
        -ExperimentalWidth $result.Width -ExperimentalHeight $result.Height
    $expectedResolution="$($case[1])x$($case[2])"
    if($applied.resolution -ne $expectedResolution -or -not $applied.windowed -or $applied.fsaa -ne 0 -or
       -not [IO.File]::ReadAllText($fixtureVideo).Contains("Resolution($($case[1]),$($case[2]))") -or
       (Get-FileHash -LiteralPath $fixtureVideo -Algorithm SHA256).Hash -ne $applied.appliedSha256) {
        throw 'Resolved pixel profile did not reach the reversible video request'
    }
    if(-not (Restore-CoJVrVideoProfile -StatePath $fixtureState -BackupPath $fixtureBackup) -or
       (Get-FileHash -LiteralPath $fixtureVideo -Algorithm SHA256).Hash -ne $originalHash) {
        throw 'Resolved pixel profile did not restore original video bytes'
    }
}
Remove-Item -LiteralPath $fixtureVideo
Remove-Item -LiteralPath $fixtureDirectory
$manual=Resolve-VrResolutionProfile -Profile Manual -Width 2804 -Height 2860 -Experimental
if($manual.Width -ne 2804 -or $manual.Height -ne 2860){throw 'Manual request was not retained'}
foreach($status in @('incompatible','unavailable')) {
    $script:CapsStatus=$status
    $fallback=Resolve-VrResolutionProfile -Profile Manual -Width 2147483647 -Height 2860 -Experimental
    if($fallback.Width -or $fallback.Height -or $fallback.Experimental -or $fallback.Status -ne 'preserve_incompatible') {throw 'Failed caps query changed resolution'}
}
$script:CapsStatus='caps_compatible'
$script:AdapterIndex=1
$fallback=Resolve-VrResolutionProfile -Profile Auto -Experimental
if($fallback.Width -or $fallback.Height -or $fallback.Experimental -or $fallback.Status -ne 'preserve_adapter_unqualified') {throw 'Unqualified adapter changed resolution'}
$script:AdapterIndex=0
foreach($operation in @(
    {Resolve-VrResolutionProfile -Profile Manual -Width 2804 -Height 2860},
    {Resolve-VrResolutionProfile -Profile Manual -Width 0 -Height 2860 -Experimental},
    {Resolve-VrResolutionProfile -Profile Manual -Width 2804.5 -Height 2860 -Experimental},
    {Resolve-VrResolutionProfile -Profile Manual -Width $true -Height 2860 -Experimental},
    {Resolve-VrResolutionProfile -Profile Preserve -Width 2804},
    {Resolve-VrResolutionProfile -Profile '75%' -Width 2804 -Height 2860 -Experimental}
)) {
    $rejected=$false;try{&$operation | Out-Null}catch{$rejected=$true};if(-not $rejected){throw 'Unqualified profile request was admitted'}
}
$script:QueryStatus='unavailable'
$fallback=Resolve-VrResolutionProfile -Profile Auto -Experimental
if($fallback.Width -or $fallback.Height -or $fallback.Experimental -or $fallback.Status -ne 'preserve_unavailable') {throw 'Unavailable runtime changed resolution'}
$script:QueryStatus='observed';$script:RecordId='foreign'
$rejected=$false;try{Resolve-VrResolutionProfile -Profile Auto -Experimental | Out-Null}catch{$rejected=$true}
if(-not $rejected){throw 'Foreign observation was admitted'}
Write-Host 'Experimental startup profiles preserve release rejection and safe fallback.'
