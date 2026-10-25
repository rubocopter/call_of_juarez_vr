param(
    [string]$SourceDirectory = (Split-Path $PSScriptRoot -Parent),
    [string]$PythonExecutable = 'python'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2
. (Join-Path $SourceDirectory 'tools/coj_video_profile.ps1')
$PolicyPath = Join-Path $SourceDirectory 'tools/coj_resolution_session.ps1'
if (-not (Test-Path -LiteralPath $PolicyPath)) { throw 'Missing read-only launch resolution policy.' }
. $PolicyPath
$script:RealInvoke = ${function:Invoke-CoJResolutionProgram}
$script:Calls = @()
$script:Ids = @()
$script:Assertions = 0
$Options = @{
    PythonExecutable = $PythonExecutable
    ResolverPath = Join-Path $SourceDirectory 'tools/coj_resolution_profiles.py'
    QueryHelperPath = 'fixture-query.exe'
    LimitsHelperPath = 'fixture-limits.exe'
}
function Assert-True([bool]$Condition, [string]$Message) {
    $script:Assertions++
    if (-not $Condition) { throw $Message }
}
function Assert-Rejected([scriptblock]$Operation, [string]$Message) {
    $Rejected = $false
    try { & $Operation | Out-Null } catch { $Rejected = $true }
    Assert-True $Rejected $Message
}
function Reset-Fixture {
    $script:Calls = @()
    $script:QueryEdit = {}
    $script:CapsEdit = {}
    $script:ProposalEdit = {}
    $script:QueryRaw = $null
    $script:CapsRaw = $null
    $script:QueryExit = 0
    $script:CapsExit = 0
    $script:FailProgram = ''
    $script:ProposalRaw = $null
}
function Invoke-CoJResolutionProgram {
    param([string]$ExecutablePath, [string[]]$Arguments, [string]$StandardInput = '',
          [int]$TimeoutMilliseconds = 30000)
    $script:Calls += $ExecutablePath
    if ($ExecutablePath -eq $script:FailProgram) { throw 'Fixture missing/failed/timed-out helper.' }
    if ($ExecutablePath -eq 'fixture-query.exe') {
        Assert-True ($Arguments.Count -eq 2 -and $Arguments[0] -ceq '--preparation-id') 'Query must request only a fresh Background observation.'
        $Id = $Arguments[1]
        Assert-True ($Id -match '^[A-Za-z0-9._-]{1,80}$' -and $Id -notin $script:Ids) 'Preparation identity was reused or invalid.'
        $script:Ids += $Id
        $Record = [pscustomobject]@{
            schemaVersion=1;recordType='cojvr-resolution-observation';status='observed'
            # A trailing zero tick must survive proposal reserialization exactly.
            preparationId=$Id;observedUtc=[DateTime]::UtcNow.ToString("yyyy-MM-dd'T'HH:mm:ss.ffffff'0Z'")
            recommendedWidth=2804;recommendedHeight=2860;runtimeVersion='fixture'
            hmdSerial='fixture';hmdModel='fixture';adapterIndex=0
            applicationKey='fixture-background';applicationKeyStatus='observed'
            applicationKeyProcessId=42;currentSceneProcessId=0;identityQualified=$false
            vrInitError=0;vrInitErrorSymbol='';error=''
        }
        & $script:QueryEdit $Record
        $Raw = $Record | ConvertTo-Json -Depth 8 -Compress
        if ($null -ne $script:QueryRaw) { $Raw = $script:QueryRaw }
        return [pscustomobject]@{ExitCode=$script:QueryExit;StdOut=$Raw;StdErr=''}
    }
    if ($ExecutablePath -eq 'fixture-limits.exe') {
        Assert-True ($Arguments.Count -eq 4 -and $Arguments[0] -ceq '--width' -and $Arguments[2] -ceq '--height') 'Unexpected caps invocation.'
        $Record = [pscustomobject]@{
            schemaVersion=1;recordType='cojvr-native-resolution-limits';status='caps_compatible'
            requestedWidth=[int]$Arguments[1];requestedHeight=[int]$Arguments[3];adapterIndex=0
            maxTextureWidth=16384;maxTextureHeight=16384;maxAspectRatio=0
            formatChecks=[pscustomobject]@{X8R8G8B8=$true;A8R8G8B8=$true;A16B16G16R16F=$true;D16=$true;D24S8=$true}
            allocationValidated=$false;memoryQualified=$false;theaterQualified=$false;error=''
        }
        & $script:CapsEdit $Record
        $Raw = $Record | ConvertTo-Json -Depth 8 -Compress
        if ($null -ne $script:CapsRaw) { $Raw = $script:CapsRaw }
        return [pscustomobject]@{ExitCode=$script:CapsExit;StdOut=$Raw;StdErr=''}
    }
    # Only Python actually runs. No native query/caps, game, video or SteamVR access.
    if ('--observation' -in $Arguments) {
        Assert-True ($Arguments -contains '-' -and $StandardInput.StartsWith('{')) 'Raw observation must use resolver stdin.'
    }
    $Result = & $script:RealInvoke @PSBoundParameters
    if ($null -ne $script:ProposalRaw) { $Result.StdOut = $script:ProposalRaw }
    else {
        # Preserve timestamp strings just as production does. PowerShell date
        # conversion otherwise trims trailing fractional zeros nondeterministically.
        $Record = ConvertFrom-CoJResolutionJson $Result.StdOut
        & $script:ProposalEdit $Record
        $Result.StdOut = $Record | ConvertTo-Json -Depth 12 -Compress
    }
    return $Result
}
function Resolve-Fixture([string]$Profile = 'Preserve', [object]$Width = 0, [object]$Height = 0) {
    Resolve-CoJLaunchResolution -Settings ([pscustomobject]@{profile=$Profile;width=$Width;height=$Height}) @Options
}
function Assert-Preserved($Result, [string]$Status) {
    Assert-True ($Result.Status -ceq $Status -and $Result.Width -eq 0 -and $Result.Height -eq 0 -and
        $Result.Experimental -is [bool] -and -not $Result.Experimental) "Unsafe fallback: $Status"
}

Reset-Fixture
Assert-Preserved (Resolve-Fixture) 'preserve'
Assert-Preserved (Resolve-CoJLaunchResolution @Options) 'preserve'
Assert-True ($script:Calls.Count -eq 0) 'Preserve invoked a subprocess.'
foreach ($Case in @(@('Auto',2804,2860),@('75%',2428,2477),@('100%',2804,2860),@('125%',3135,3198),@('Manual',3200,2400))) {
    Reset-Fixture
    $Result = if ($Case[0] -eq 'Manual') { Resolve-Fixture Manual $Case[1] $Case[2] } else { Resolve-Fixture $Case[0] }
    Assert-True ($Result.Status -ceq 'experimental_trial' -and $Result.Experimental -and
        $Result.Profile -ceq $Case[0] -and $Result.Width -eq $Case[1] -and $Result.Height -eq $Case[2]) 'Selected profile lost its experimental request.'
    Assert-True ($Result.Proposal.applyAllowed -is [bool] -and -not $Result.Proposal.applyAllowed -and
        -not $Result.Limits.memoryQualified -and -not $Result.Limits.allocationValidated -and
        -not $Result.Limits.theaterQualified -and $null -eq $Result.Proposal.totalMemoryEstimateBytes) 'Trial promoted physical qualification.'
    Assert-True (($script:Calls -contains 'fixture-query.exe') -eq ($Case[0] -ne 'Manual')) 'Manual queried the runtime.'
}
foreach ($Case in @(@('Manual',0,1),@('Manual',1,0),@('Manual',-1,1),@('Manual',1.0,1),
        @('Manual',$true,1),@('Manual',[decimal]1,1),@('Manual',[single]1,1),@('Manual',$null,1),
        @('Manual','1.0',1),@('Manual','2147483648',1),@('Manual',[uint64]::MaxValue,1),
        @('Auto',1,1),@('75%',1,0),@('Preserve',1,0),@('unknown',0,0))) {
    Reset-Fixture
    Assert-Rejected { Resolve-Fixture $Case[0] $Case[1] $Case[2] } 'Invalid profile/dimensions admitted.'
    Assert-True ($script:Calls.Count -eq 0) 'Invalid settings invoked a helper.'
}
Assert-Rejected { Resolve-CoJLaunchResolution -Settings ([pscustomobject]@{profile=$true;width=0;height=0}) @Options } 'Boolean profile admitted.'
Assert-Rejected { Resolve-CoJLaunchResolution -Settings ([pscustomobject]@{profile='Manual';width=1}) @Options } 'Missing settings dimension admitted.'

foreach ($Program in @('fixture-query.exe', 'fixture-limits.exe', $PythonExecutable)) {
    Reset-Fixture
    $script:FailProgram = $Program
    Assert-Preserved (Resolve-Fixture Auto) 'preserve_unavailable'
}
Reset-Fixture
$MissingResolver = $Options.Clone()
$MissingResolver.ResolverPath = Join-Path $SourceDirectory 'work/resolution-selector-20261017/does-not-exist.py'
Assert-Preserved (Resolve-CoJLaunchResolution -Settings @{profile='Manual';width=100;height=100} @MissingResolver) 'preserve_unavailable'
foreach ($Program in @('query','caps')) {
    Reset-Fixture
    if ($Program -eq 'query') { $script:QueryExit=99 } else { $script:CapsExit=99 }
    Assert-Preserved (Resolve-Fixture Auto) 'preserve_unavailable'
}
Reset-Fixture
$script:QueryEdit = { param($r) $r.status='unavailable';$r.adapterIndex=-1;$r.recommendedWidth=0;$r.recommendedHeight=0 }
$script:QueryExit=2
Assert-Preserved (Resolve-Fixture Auto) 'preserve_unavailable'
Assert-True ($script:Calls -notcontains 'fixture-limits.exe') 'Unavailable observation reached caps.'
foreach ($Status in @('unavailable','incompatible')) {
    Reset-Fixture
    $script:CapsEdit = { param($r) $r.status=$Status;$r.error='fixture_unavailable_or_incompatible' }
    $script:CapsExit=2
    $Expected = if ($Status -eq 'unavailable') { 'preserve_unavailable' } else { 'preserve_incompatible' }
    Assert-Preserved (Resolve-Fixture Manual 3200 2400) $Expected
}
Reset-Fixture
$script:QueryEdit = {param($r) $r.adapterIndex=1}
Assert-Preserved (Resolve-Fixture Auto) 'preserve_adapter_unqualified'
Assert-True ($script:Calls -notcontains 'fixture-limits.exe') 'Foreign runtime adapter reached native caps.'
Reset-Fixture
$script:CapsEdit = {param($r) $r.adapterIndex=1}
Assert-Preserved (Resolve-Fixture Manual 3200 2400) 'preserve_adapter_unqualified'

# These are rejected by the actual resolver, including strict raw JSON parsing.
foreach ($Edit in @(
    {param($r) $r.preparationId='foreign'},
    {param($r) $r.observedUtc=[DateTime]::UtcNow.AddMinutes(-3).ToString('o')},
    {param($r) $r.observedUtc=[DateTime]::UtcNow.AddMinutes(1).ToString('o')},
    {param($r) $r.identityQualified=$true},
    {param($r) $r.recommendedWidth=$true},
    {param($r) $r.status='foreign'}
)) {
    Reset-Fixture; $script:QueryEdit=$Edit
    Assert-Rejected { Resolve-Fixture Auto } "Malformed/foreign/stale observation admitted: $Edit"
}
foreach ($Raw in @('{"schemaVersion":1,"schemaVersion":1}', '{"x":NaN}', '[]', '{} trailing', (' ' * 65537))) {
    Reset-Fixture; $script:QueryRaw=$Raw
    Assert-Rejected { Resolve-Fixture Auto } 'Invalid raw observation admitted.'
}
foreach ($Edit in @(
    {param($r) $r.schemaVersion=$true}, {param($r) $r.recordType='foreign'},
    {param($r) $r.requestedProfile='75%'}, {param($r) $r.applyAllowed=$true},
    {param($r) $r.applyAllowed=0}, {param($r) $r.candidateDimensions=@(3201,2400)},
    {param($r) $r.candidateDimensions=@('3200',2400)},
    {param($r) $r.payloadLowerBoundBytes=1}, {param($r) $r.effectiveEyeDimensions=@(3200,2400)},
    {param($r) $r.totalMemoryEstimateBytes=1}, {param($r) $r.status='supported'}
)) {
    Reset-Fixture; $script:ProposalEdit=$Edit
    Assert-Rejected { Resolve-Fixture Manual 3200 2400 } 'Invalid proposal admitted.'
}
Reset-Fixture
$script:ProposalEdit={param($r) $r.observation.preparationId='foreign'}
Assert-Rejected { Resolve-Fixture Auto } 'Proposal lost observation correlation.'
foreach ($Edit in @(
    {param($r) $r.schemaVersion=$true}, {param($r) $r.requestedWidth++},
    {param($r) $r.requestedHeight='2400'}, {param($r) $r.adapterIndex=$false},
    {param($r) $r.allocationValidated=$true}, {param($r) $r.memoryQualified=0},
    {param($r) $r.theaterQualified=$true}, {param($r) $r.status='unavailable'},
    {param($r) $r.maxTextureWidth=10}, {param($r) $r.formatChecks.D16=$false}
)) {
    Reset-Fixture; $script:CapsEdit=$Edit
    Assert-Rejected { Resolve-Fixture Manual 3200 2400 } 'Malformed/inconsistent caps report admitted.'
}
foreach ($Raw in @('{}', '[]', '{"schemaVersion":1,"schemaVersion":1}', '{"schemaVersion":1,}', 'not JSON')) {
    Reset-Fixture; $script:CapsRaw=$Raw
    Assert-Rejected { Resolve-Fixture Manual 3200 2400 } 'Invalid caps JSON admitted.'
    Reset-Fixture; $script:ProposalRaw=$Raw
    Assert-Rejected { Resolve-Fixture Manual 3200 2400 } 'Invalid proposal JSON admitted.'
}

# Current implementation bounds, with wide arithmetic; never a universal 2804 cap.
foreach ($Case in @(@(16384,1,$true),@(16385,1,$false),@(8192,4096,$true),@(8192,4097,$false),
        @(16384,8192,$false),@(2147483647,1,$false),@(65536,65536,$false))) {
    Reset-Fixture
    $Result=Resolve-Fixture Manual $Case[0] $Case[1]
    if ($Case[2]) { Assert-True ($Result.Experimental -and $Result.Width -eq $Case[0]) 'Necessary bound rejected a bounded trial.' }
    else {
        Assert-Preserved $Result 'preserve_incompatible'
        Assert-True ($script:Calls -notcontains 'fixture-limits.exe') 'Oversize candidate reached native caps.'
    }
}
Reset-Fixture
Assert-Rejected { Resolve-Fixture Manual 2147483647 2147483647 } 'Resolver uint64 payload overflow was admitted.'

# Exercise the real process seam with harmless Python only: quoting, separate
# streams, UTF-8 stdin, failure codes and bounded termination of this child.
$Probe = & $script:RealInvoke -ExecutablePath $PythonExecutable -Arguments @('-c',
    'import sys; print(sys.stdin.read()); print(sys.argv[1]); print("diagnostic",file=sys.stderr); sys.exit(2)',
    'space "quote" trailing\') -StandardInput ('fixture-' + [char]0x00e9)
Assert-True ($Probe.ExitCode -eq 2 -and $Probe.StdOut.Contains('space "quote" trailing\') -and
    $Probe.StdOut.Contains(('fixture-' + [char]0x00e9)) -and $Probe.StdErr.Contains('diagnostic')) 'Process seam corrupted streams/arguments.'
$Timer=[Diagnostics.Stopwatch]::StartNew()
Assert-Rejected { & $script:RealInvoke -ExecutablePath $PythonExecutable -Arguments @('-c','import time; time.sleep(10)') -TimeoutMilliseconds 100 } 'Child helper timeout was not enforced.'
Assert-True ($Timer.ElapsedMilliseconds -lt 5000) 'Child timeout exceeded its bound.'
Write-Host "PASS: $script:Assertions assertions; host-only resolution policy and actual Python resolver/process seam."
