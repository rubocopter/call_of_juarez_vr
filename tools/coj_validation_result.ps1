# Diagnostics-owned evidence. These results describe canonical telemetry checks;
# neither collection nor operator observations can promote a physical gate.
function Get-CoJValidationContext([string]$GameDirectory) {
    $GameDirectory = [IO.Path]::GetFullPath($GameDirectory)
    $RunPath = Join-Path $GameDirectory '.cojvr-run.json'
    $Run = Get-Content -LiteralPath $RunPath -Raw | ConvertFrom-Json
    $RunId = [string]$Run.runId
    if ([string]::IsNullOrWhiteSpace($RunId) -or $RunId -notmatch '^[A-Za-z0-9._-]+$') {
        throw 'Cannot bind a validation result to an invalid run ID.'
    }
    $RunDirectory = Join-Path $GameDirectory ".cojvr-evidence/runs/$RunId"
    if (-not (Test-Path -LiteralPath $RunDirectory -PathType Container)) {
        throw 'Cannot bind a validation result without the staged run evidence directory.'
    }
    $Inputs = [Collections.Generic.List[object]]::new()
    $Paths = @('.cojvr-run.json', '.cojvr-d3d9-stage.json', 'cojvr.log', ".cojvr-evidence/runs/$RunId/build-manifest.json")
    $Paths += @($Run.deployment | ForEach-Object { [string]$_.destination })
    foreach ($Relative in $Paths) {
        $Path = Join-Path $GameDirectory $Relative
        $Exists = Test-Path -LiteralPath $Path -PathType Leaf
        $Inputs.Add([ordered]@{
            scope = 'game'; path = $Relative.Replace('\', '/'); exists = $Exists
            sha256 = if ($Exists) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash } else { $null }
            size = if ($Exists) { (Get-Item -LiteralPath $Path).Length } else { $null }
        })
    }
    foreach ($Name in @('verify_native_stereo_live_test.ps1', 'get_run_provenance.ps1', 'coj_executable_identity.ps1', 'coj_validation_result.ps1')) {
        $Path = Join-Path $PSScriptRoot $Name
        $Inputs.Add([ordered]@{
            scope = 'tools'; path = $Name; exists = $true
            sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
            size = (Get-Item -LiteralPath $Path).Length
        })
    }
    $Build = $null
    try { $Build = Get-Content -LiteralPath (Join-Path $RunDirectory 'build-manifest.json') -Raw | ConvertFrom-Json } catch {
        # Canonical provenance validation supplies the rejection; retain hashes
        # even if this untrusted metadata is unreadable.
    }
    $Lines = @()
    if (Test-Path -LiteralPath (Join-Path $GameDirectory 'cojvr.log') -PathType Leaf) {
        $Lines = @(Get-Content -LiteralPath (Join-Path $GameDirectory 'cojvr.log'))
    }
    $EscapedRun = [regex]::Escape($RunId)
    $EscapedBuild = [regex]::Escape([string]$Run.buildManifestId)
    [pscustomobject]@{
        Run = $Run; Build = $Build; Inputs = @($Inputs)
        RunDirectory = $RunDirectory; ResultPath = Join-Path $RunDirectory 'analysis/validation-result.json'
        Profile = if ([string]::IsNullOrWhiteSpace([string]$Run.validation.profile)) {
            if ($Run.diagnosticMode -eq 'd3d9_native_stereo') { 'full' } else { 'default' }
        } else { [string]$Run.validation.profile }
        RuntimeStarted = [bool]($Lines -match "^run_start: run_id=$EscapedRun build_manifest_id=$EscapedBuild(?:\s|$)")
        RuntimeEnded = [bool]($Lines -match "^run_end: run_id=$EscapedRun(?:\s|$)")
    }
}

function Write-CoJValidationResult(
    [object]$Context, [bool]$Executed, [bool]$Succeeded, [object[]]$Reasons) {
    $Outcome = if (-not $Executed) { 'inconclusive' } elseif ($Succeeded) { 'pass' } else { 'fail' }
    if ($Outcome -eq 'pass' -and (-not $Context.RuntimeStarted -or -not $Context.RuntimeEnded)) {
        $Outcome = 'inconclusive'
        $Reasons = @($Reasons) + @([ordered]@{code='partial_run'; message='A complete correlated runtime start/end is required.'})
    }
    $Result = [ordered]@{
        schemaVersion = 1; manifestType = 'cojvr-validation-result'
        runId = [string]$Context.Run.runId; buildManifestId = [string]$Context.Run.buildManifestId
        buildManifestSha256 = [string]$Context.Run.buildManifestSha256
        diagnosticMode = [string]$Context.Run.diagnosticMode
        source = $Context.Build.source; build = $Context.Build.build
        validationProfile = $Context.Profile; requirements = $Context.Run.validation
        evaluatedUtc = [DateTime]::UtcNow.ToString('o')
        verifier = [ordered]@{
            name = 'verify_native_stereo_live_test.ps1'
            sha256 = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'verify_native_stereo_live_test.ps1')).Hash
            executed = $Executed; succeeded = $Succeeded
        }
        outcome = $Outcome; reasons = @($Reasons)
        runtimeStarted = $Context.RuntimeStarted; runtimeEnded = $Context.RuntimeEnded
        scope = 'canonical_telemetry_checks_only'; headsetAcceptance = 'not_evaluated'
        inputs = $Context.Inputs
    }
    $Directory = [IO.Path]::GetDirectoryName($Context.ResultPath)
    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    $Temporary = $Context.ResultPath + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    try {
        [IO.File]::WriteAllText($Temporary, ($Result | ConvertTo-Json -Depth 20) + [Environment]::NewLine,
            [Text.UTF8Encoding]::new($false))
        [IO.File]::Move($Temporary, $Context.ResultPath, $true)
    } finally {
        if (Test-Path -LiteralPath $Temporary) { Remove-Item -LiteralPath $Temporary -Force }
    }
}

function Assert-CoJValidationResult([object]$Context, [object]$Result) {
    $SameInputs = ($Result.inputs | ConvertTo-Json -Depth 20 -Compress) -ceq
        ($Context.Inputs | ConvertTo-Json -Depth 20 -Compress)
    if (-not $SameInputs -or $Result.runId -cne $Context.Run.runId -or
        $Result.buildManifestId -cne $Context.Run.buildManifestId -or
        $Result.buildManifestSha256 -cne $Context.Run.buildManifestSha256 -or
        $Result.diagnosticMode -cne $Context.Run.diagnosticMode -or
        $Result.validationProfile -cne $Context.Profile -or
        ($Result.requirements | ConvertTo-Json -Depth 20 -Compress) -cne
            ($Context.Run.validation | ConvertTo-Json -Depth 20 -Compress) -or
        ($Result.source | ConvertTo-Json -Depth 20 -Compress) -cne
            ($Context.Build.source | ConvertTo-Json -Depth 20 -Compress) -or
        ($Result.build | ConvertTo-Json -Depth 20 -Compress) -cne
            ($Context.Build.build | ConvertTo-Json -Depth 20 -Compress)) {
        throw 'The validation result does not match the current run/build/profile/inputs; rerun the canonical verifier.'
    }
    if ($Result.schemaVersion -ne 1 -or $Result.manifestType -ne 'cojvr-validation-result' -or
        $Result.outcome -notin @('pass', 'fail', 'inconclusive') -or
        $Result.scope -ne 'canonical_telemetry_checks_only' -or $Result.headsetAcceptance -ne 'not_evaluated' -or
        ($Result.outcome -eq 'pass' -and (-not $Result.verifier.executed -or -not $Result.verifier.succeeded -or
            -not $Context.RuntimeStarted -or -not $Context.RuntimeEnded -or @($Result.reasons).Count -ne 0)) -or
        ($Result.outcome -eq 'fail' -and (-not $Result.verifier.executed -or $Result.verifier.succeeded)) -or
        ($Result.outcome -ne 'pass' -and @($Result.reasons).Count -eq 0)) {
        throw 'The validation result has an invalid or contradictory verdict.'
    }
}
