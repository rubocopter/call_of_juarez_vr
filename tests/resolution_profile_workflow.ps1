param([string]$SourceDirectory,[string]$BinaryDirectory)
$ErrorActionPreference='Stop'
$RepositoryRoot=$SourceDirectory
. (Join-Path $SourceDirectory 'tools/coj_video_profile.ps1')
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $SourceDirectory 'tools/vr_test.ps1'),[ref]$null,[ref]$null)
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
foreach($case in @(@('75%',2428,2477),@('100%',2804,2860),@('125%',3135,3198),@('Auto',2804,2860))) {
    $result=Resolve-VrResolutionProfile -Profile $case[0] -Experimental
    if($result.Width -ne $case[1] -or $result.Height -ne $case[2] -or -not $result.Experimental -or $result.Proposal.applyAllowed) {throw 'Experimental pixel profile mismatch'}
}
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
