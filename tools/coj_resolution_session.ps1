# Read-only startup policy. Dot-source coj_video_profile.ps1 first for its
# ConvertTo-CoJNativeDimension contract. No video/journal/runtime state is owned here.
function Invoke-CoJResolutionProgram {
    param(
        [Parameter(Mandatory=$true)][string]$ExecutablePath,
        [string[]]$Arguments = @(),
        [string]$StandardInput = '',
        [ValidateRange(1,30000)][int]$TimeoutMilliseconds = 30000
    )
    $Start = New-Object Diagnostics.ProcessStartInfo
    $Start.FileName = $ExecutablePath
    # Windows argv quoting, including embedded quotes and trailing backslashes.
    $Start.Arguments = (@($Arguments | ForEach-Object {
        '"' + (($_ -replace '(\\*)"', '$1$1\"') -replace '(\\+)$', '$1$1') + '"'
    }) -join ' ')
    $Start.UseShellExecute = $false
    $Start.CreateNoWindow = $true
    $Start.RedirectStandardInput = $true
    $Start.RedirectStandardOutput = $true
    $Start.RedirectStandardError = $true
    $Start.StandardOutputEncoding = New-Object Text.UTF8Encoding($false)
    $Start.StandardErrorEncoding = New-Object Text.UTF8Encoding($false)
    $Start.EnvironmentVariables['PYTHONIOENCODING'] = 'utf-8'
    $Process = New-Object Diagnostics.Process
    $Process.StartInfo = $Start
    $Clock = [Diagnostics.Stopwatch]::StartNew()
    $Started = $false
    try {
        $Started = $Process.Start()
        if (-not $Started) { throw 'Resolution helper did not start.' }
        $Out = $Process.StandardOutput.ReadToEndAsync()
        $Err = $Process.StandardError.ReadToEndAsync()
        $Bytes = [Text.Encoding]::UTF8.GetBytes($StandardInput)
        $Write = $Process.StandardInput.BaseStream.WriteAsync($Bytes,0,$Bytes.Length)
        if (-not $Write.Wait([Math]::Max(1,$TimeoutMilliseconds-[int]$Clock.ElapsedMilliseconds))) {
            throw 'Resolution helper stdin timed out.'
        }
        $Process.StandardInput.Close()
        if (-not $Process.WaitForExit([Math]::Max(1,$TimeoutMilliseconds-[int]$Clock.ElapsedMilliseconds)) -or
            -not $Out.Wait([Math]::Max(1,$TimeoutMilliseconds-[int]$Clock.ElapsedMilliseconds)) -or
            -not $Err.Wait([Math]::Max(1,$TimeoutMilliseconds-[int]$Clock.ElapsedMilliseconds))) {
            throw 'Resolution helper timed out.'
        }
        return [pscustomobject]@{ExitCode=$Process.ExitCode;StdOut=$Out.Result;StdErr=$Err.Result}
    } finally {
        # Only this child helper may be terminated, never the game or SteamVR.
        if ($Started -and -not $Process.HasExited) { $Process.Kill() }
        $Process.Dispose()
    }
}

function ConvertFrom-CoJResolutionJson {
    param([string]$Json)
    if ([string]::IsNullOrWhiteSpace($Json) -or [Text.Encoding]::UTF8.GetByteCount($Json) -gt 65536) {
        throw 'Resolution report is empty or exceeds 64 KiB.'
    }
    # ConvertFrom-Json alone accepts duplicate keys (and on some hosts comments
    # or trailing commas). Validate bounded JSON grammar before converting on 5.1.
    $Pattern = '"(?:[^"\\\x00-\x1F]|\\(?:["\\/bfnrt]|u[0-9a-fA-F]{4}))*"|-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?|true|false|null|[{}\[\]:,]|\S'
    $State = @{Tokens=[regex]::Matches($Json,$Pattern);Index=0}
    function Read-CoJJsonValue([int]$Depth) {
        if ($Depth -gt 16 -or $State.Index -ge $State.Tokens.Count) { throw 'Invalid resolution JSON nesting/value.' }
        $Token=$State.Tokens[$State.Index++].Value
        if ($Token -eq '{' -or $Token -eq '[') {
            $Object=$Token -eq '{'
            $End=if ($Object) {'}'} else {']'}
            $Keys=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
            if ($State.Index -lt $State.Tokens.Count -and $State.Tokens[$State.Index].Value -eq $End) { $State.Index++; return }
            while ($true) {
                if ($Object) {
                    if ($State.Index -ge $State.Tokens.Count) { throw 'Missing JSON key.' }
                    $KeyToken=$State.Tokens[$State.Index++].Value
                    if (-not $KeyToken.StartsWith('"')) { throw 'Invalid JSON key.' }
                    $Key=(ConvertFrom-Json -InputObject ('{"key":'+$KeyToken+'}') -ErrorAction Stop).key
                    if (-not $Keys.Add($Key)) { throw 'Duplicate resolution JSON key.' }
                    if ($State.Index -ge $State.Tokens.Count -or $State.Tokens[$State.Index++].Value -ne ':') { throw 'Missing JSON colon.' }
                }
                Read-CoJJsonValue ($Depth+1)
                if ($State.Index -ge $State.Tokens.Count) { throw 'Unterminated JSON container.' }
                $Next=$State.Tokens[$State.Index++].Value
                if ($Next -eq $End) { return }
                if ($Next -ne ',') { throw 'Invalid JSON separator.' }
            }
        }
        if ($Token -notmatch '^(?:"(?:[^"\\\x00-\x1F]|\\(?:["\\/bfnrt]|u[0-9a-fA-F]{4}))*"|-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?|true|false|null)$') {
            throw 'Invalid JSON scalar.'
        }
    }
    Read-CoJJsonValue 0
    if ($State.Index -ne $State.Tokens.Count -or $State.Tokens[0].Value -ne '{') { throw 'Resolution report must be one JSON object.' }
    $Options=@{InputObject=$Json;ErrorAction='Stop'}
    if ((Get-Command ConvertFrom-Json).Parameters.ContainsKey('DateKind')) { $Options.DateKind='String' }
    return ConvertFrom-Json @Options
}

function Test-CoJResolutionInteger {
    param($Value, [decimal]$Minimum=0, [decimal]$Maximum=2147483647)
    return (($Value -is [int] -or $Value -is [long] -or $Value -is [uint32] -or $Value -is [uint64] -or
        $Value -is [int16] -or $Value -is [uint16] -or $Value -is [byte] -or $Value -is [sbyte]) -and
        [decimal]$Value -ge $Minimum -and [decimal]$Value -le $Maximum)
}

function Assert-CoJResolutionFields {
    param($Record, [string[]]$Fields)
    if ($null -eq $Record -or $Record -isnot [pscustomobject]) { throw 'Resolution report must be an object.' }
    foreach ($Field in $Fields) {
        if ($null -eq $Record.PSObject.Properties[$Field]) { throw "Missing resolution report field: $Field" }
    }
}

function Resolve-CoJLaunchResolution {
    param(
        [object]$Settings = ([pscustomobject]@{profile='Preserve';width=0;height=0}),
        [string]$PythonExecutable,
        [string]$ResolverPath,
        [string]$QueryHelperPath,
        [string]$LimitsHelperPath
    )
    foreach ($Field in @('profile','width','height')) {
        if ($null -eq $Settings -or
            ($Settings -is [Collections.IDictionary] -and -not $Settings.Contains($Field)) -or
            ($Settings -isnot [Collections.IDictionary] -and $null -eq $Settings.PSObject.Properties[$Field])) {
            throw "Missing launch resolution setting: $Field"
        }
    }
    $Profile=$Settings.profile
    if ($Profile -isnot [string] -or @('Preserve','Auto','75%','100%','125%','Manual') -cnotcontains $Profile) {
        throw 'Invalid launch resolution profile.'
    }
    $Width=ConvertTo-CoJNativeDimension $Settings.width
    $Height=ConvertTo-CoJNativeDimension $Settings.height
    if (($Profile -eq 'Manual' -and (-not $Width -or -not $Height)) -or
        ($Profile -ne 'Manual' -and ($Width -or $Height))) { throw 'Only Manual accepts dimensions, and requires both positive axes.' }
    $Decision=[pscustomobject]@{Status='preserve';Width=0;Height=0;Experimental=$false;Profile=$Profile;Proposal=$null;Limits=$null}
    if ($Profile -eq 'Preserve') { return $Decision }
    $Decision.Status='preserve_unavailable'
    $Decision.Proposal=[pscustomobject]@{status='unavailable';applyAllowed=$false;reason='resolver_unavailable'}
    if ([string]::IsNullOrWhiteSpace($ResolverPath) -or -not (Test-Path -LiteralPath $ResolverPath -PathType Leaf)) { return $Decision }
    $Arguments=@($ResolverPath,'--profile',$Profile)
    $Observation=$null
    $InputJson=''
    if ($Profile -eq 'Manual') { $Arguments+=@('--width',[string]$Width,'--height',[string]$Height) }
    else {
        $PreparationId='resolution-'+[guid]::NewGuid().ToString('N')
        try { $Query=Invoke-CoJResolutionProgram -ExecutablePath $QueryHelperPath -Arguments @('--preparation-id',$PreparationId) }
        catch { $Decision.Proposal.reason='query_helper_unavailable: '+$_.Exception.Message; return $Decision }
        if ($Query.ExitCode -notin @(0,2)) { $Decision.Proposal.reason='query_helper_failed'; return $Decision }
        $InputJson=$Query.StdOut
        # Retain the original JSON for Python's duplicate/nonfinite/freshness checks.
        $Observation=ConvertFrom-CoJResolutionJson $InputJson
        Assert-CoJResolutionFields $Observation @('schemaVersion','recordType','status','preparationId','observedUtc',
            'recommendedWidth','recommendedHeight','runtimeVersion','hmdSerial','hmdModel','adapterIndex',
            'applicationKey','applicationKeyStatus','identityQualified')
        if (-not (Test-CoJResolutionInteger $Observation.schemaVersion 1 1) -or
            $Observation.recordType -cne 'cojvr-resolution-observation' -or
            $Observation.status -cnotin @('observed','unavailable') -or
            $Observation.preparationId -cne $PreparationId -or
            $Observation.identityQualified -isnot [bool] -or $Observation.identityQualified -or
            (($Query.ExitCode -eq 0) -ne ($Observation.status -ceq 'observed'))) { throw 'Invalid/correlated observation state.' }
        $Arguments+=@('--observation','-','--preparation-id',$PreparationId)
    }
    try { $Result=Invoke-CoJResolutionProgram -ExecutablePath $PythonExecutable -Arguments $Arguments -StandardInput $InputJson }
    catch { $Decision.Proposal.reason='resolver_unavailable: '+$_.Exception.Message; return $Decision }
    if ($Result.ExitCode -eq 1) { throw ('Resolution request rejected: '+$Result.StdOut) }
    if ($Result.ExitCode -ne 0) { $Decision.Proposal.reason='resolver_failed'; return $Decision }
    $Proposal=ConvertFrom-CoJResolutionJson $Result.StdOut
    Assert-CoJResolutionFields $Proposal @('schemaVersion','recordType','requestedProfile','status','candidateDimensions',
        'applyAllowed','effectiveEngineDimensions','effectiveEyeDimensions','compositorSubmitDimensions','fallback',
        'unresolvedGates','errors','payloadLowerBoundBytes','totalMemoryEstimateBytes','limitations')
    if (-not (Test-CoJResolutionInteger $Proposal.schemaVersion 1 1) -or
        $Proposal.recordType -cne 'cojvr-resolution-proposal' -or $Proposal.requestedProfile -cne $Profile -or
        $Proposal.status -cnotin @('proposal_only','unavailable') -or
        $Proposal.applyAllowed -isnot [bool] -or $Proposal.applyAllowed -or
        $Proposal.fallback -cne 'preserve_selected_native_video' -or
        $null -ne $Proposal.effectiveEngineDimensions -or $null -ne $Proposal.effectiveEyeDimensions -or
        $null -ne $Proposal.compositorSubmitDimensions -or $null -ne $Proposal.totalMemoryEstimateBytes -or
        @($Proposal.errors).Count -ne 0 -or @($Proposal.unresolvedGates).Count -eq 0) { throw 'Invalid resolution proposal admission state.' }
    if ($Profile -ne 'Manual') {
        Assert-CoJResolutionFields $Proposal @('observation')
        if (($Proposal.observation | ConvertTo-Json -Depth 12 -Compress) -cne
            ($Observation | ConvertTo-Json -Depth 12 -Compress)) { throw 'Proposal observation does not match this preparation.' }
    }
    $Decision.Proposal=$Proposal
    if ($Proposal.status -eq 'unavailable') {
        if ($Profile -eq 'Manual' -or $Observation.status -ne 'unavailable' -or $null -ne $Proposal.candidateDimensions) {
            throw 'Inconsistent unavailable proposal.'
        }
        return $Decision
    }
    Assert-CoJResolutionFields $Proposal @('pixelCount')
    if ($Proposal.candidateDimensions -isnot [array] -or $Proposal.candidateDimensions.Count -ne 2 -or
        -not (Test-CoJResolutionInteger $Proposal.candidateDimensions[0] 1) -or
        -not (Test-CoJResolutionInteger $Proposal.candidateDimensions[1] 1)) { throw 'Invalid candidate dimensions.' }
    if ($Profile -ne 'Manual') {
        if ($Observation.status -ne 'observed' -or -not (Test-CoJResolutionInteger $Observation.adapterIndex) -or
            -not (Test-CoJResolutionInteger $Observation.recommendedWidth 1) -or
            -not (Test-CoJResolutionInteger $Observation.recommendedHeight 1)) { throw 'Invalid observed dimensions/adapter.' }
        $Ratio=switch ($Profile) {'75%' {0.75} '125%' {1.25} default {1.0}}
        $Width=[Math]::Floor($Observation.recommendedWidth*[Math]::Sqrt($Ratio)+0.5)
        $Height=[Math]::Floor($Observation.recommendedHeight*[Math]::Sqrt($Ratio)+0.5)
    }
    if ($Proposal.candidateDimensions[0] -ne $Width -or $Proposal.candidateDimensions[1] -ne $Height) { throw 'Proposal changed the requested extent.' }
    $Width=[long]$Proposal.candidateDimensions[0]
    $Height=[long]$Proposal.candidateDimensions[1]
    # Decimal avoids overflow even for two native INT_MAX axes times 32.
    $Pixels=[decimal]$Width*[decimal]$Height
    if (-not (Test-CoJResolutionInteger $Proposal.pixelCount 1 ([long]::MaxValue)) -or
        [decimal]$Proposal.pixelCount -ne $Pixels -or
        -not (Test-CoJResolutionInteger $Proposal.payloadLowerBoundBytes 1 ([uint64]::MaxValue)) -or
        [decimal]$Proposal.payloadLowerBoundBytes -ne 32*$Pixels) { throw 'Invalid nominal payload arithmetic.' }
    if ($Profile -ne 'Manual' -and $Observation.adapterIndex -ne 0) { $Decision.Status='preserve_adapter_unqualified'; return $Decision }
    # Necessary current implementation limits only. These do not bound total
    # native/theater memory or qualify recommendation identity or physical support.
    $Reason=if ($Width -gt 16384 -or $Height -gt 16384) {'implementation_axis_limit_exceeded'}
        elseif (4*$Pixels -gt 134217728) {'menu_source_128mib_limit_exceeded'}
        elseif (32*$Pixels -ge 4294967296) {'nominal_payload_reaches_x86_laa_ceiling'} else {''}
    if ($Reason) {
        $Decision.Status='preserve_incompatible'
        $Decision.Limits=[pscustomobject]@{status='incompatible';requestedWidth=$Width;requestedHeight=$Height;
            error=$Reason;allocationValidated=$false;memoryQualified=$false;theaterQualified=$false}
        return $Decision
    }
    try { $Native=Invoke-CoJResolutionProgram -ExecutablePath $LimitsHelperPath -Arguments @('--width',[string]$Width,'--height',[string]$Height) }
    catch {
        $Decision.Limits=[pscustomobject]@{status='unavailable';error=$_.Exception.Message}
        return $Decision
    }
    if ($Native.ExitCode -notin @(0,2)) {
        $Decision.Limits=[pscustomobject]@{status='unavailable';error='native_limits_helper_failed'}
        return $Decision
    }
    $Limits=ConvertFrom-CoJResolutionJson $Native.StdOut
    Assert-CoJResolutionFields $Limits @('schemaVersion','recordType','status','requestedWidth','requestedHeight',
        'adapterIndex','maxTextureWidth','maxTextureHeight','maxAspectRatio','formatChecks',
        'allocationValidated','memoryQualified','theaterQualified','error')
    if (-not (Test-CoJResolutionInteger $Limits.schemaVersion 1 1) -or
        $Limits.recordType -cne 'cojvr-native-resolution-limits' -or
        $Limits.status -cnotin @('caps_compatible','incompatible','unavailable') -or
        -not (Test-CoJResolutionInteger $Limits.requestedWidth 1) -or $Limits.requestedWidth -ne $Width -or
        -not (Test-CoJResolutionInteger $Limits.requestedHeight 1) -or $Limits.requestedHeight -ne $Height -or
        -not (Test-CoJResolutionInteger $Limits.adapterIndex) -or
        (($Native.ExitCode -eq 0) -ne ($Limits.status -eq 'caps_compatible'))) { throw 'Inconsistent native limits correlation/state.' }
    foreach ($Flag in @('allocationValidated','memoryQualified','theaterQualified')) {
        if ($Limits.$Flag -isnot [bool] -or $Limits.$Flag) { throw 'Native caps cannot qualify allocation/memory/theater.' }
    }
    foreach ($Field in @('maxTextureWidth','maxTextureHeight','maxAspectRatio')) {
        if (-not (Test-CoJResolutionInteger $Limits.$Field 0 ([uint32]::MaxValue))) { throw 'Invalid native caps integer.' }
    }
    Assert-CoJResolutionFields $Limits.formatChecks @('X8R8G8B8','A8R8G8B8','A16B16G16R16F','D16','D24S8')
    foreach ($Format in @('X8R8G8B8','A8R8G8B8','A16B16G16R16F','D16','D24S8')) {
        $Check=$Limits.formatChecks.$Format
        if (($null -ne $Check -and $Check -isnot [bool]) -or
            ($Limits.status -eq 'caps_compatible' -and ($Check -isnot [bool] -or -not $Check))) { throw 'Invalid native format report.' }
    }
    if ($Limits.status -eq 'caps_compatible' -and
        ($Limits.maxTextureWidth -lt $Width -or $Limits.maxTextureHeight -lt $Height -or
         ($Limits.maxAspectRatio -gt 0 -and ($Width -gt [decimal]$Height*$Limits.maxAspectRatio -or
                                           $Height -gt [decimal]$Width*$Limits.maxAspectRatio)))) { throw 'Native caps contradict compatible status.' }
    $Decision.Limits=$Limits
    if ($Limits.adapterIndex -ne 0) { $Decision.Status='preserve_adapter_unqualified'; return $Decision }
    if ($Limits.status -ne 'caps_compatible') {
        if ($Limits.status -eq 'incompatible') { $Decision.Status='preserve_incompatible' }
        return $Decision
    }
    $Decision.Status='experimental_trial'
    $Decision.Width=[int]$Width
    $Decision.Height=[int]$Height
    $Decision.Experimental=$true
    return $Decision
}
