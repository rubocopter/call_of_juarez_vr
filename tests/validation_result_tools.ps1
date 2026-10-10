param(
    [Parameter(Mandatory = $true)][string]$SourceDirectory,
    [Parameter(Mandatory = $true)][string]$BinaryDirectory
)
$ErrorActionPreference = 'Stop'
function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Write-Json([string]$Path, [object]$Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 20), [Text.UTF8Encoding]::new($false))
}
function Assert-Rejected([scriptblock]$Action, [string]$Pattern) {
    $Message = $null
    try { & $Action | Out-Null } catch { $Message = $_.Exception.Message }
    Assert-True ($Message -match $Pattern) "Expected rejection matching '$Pattern', got '$Message'."
}
$Root = [IO.Path]::GetFullPath((Join-Path $BinaryDirectory ('validation-result-' + [Guid]::NewGuid().ToString('N'))))
$Boundary = [IO.Path]::GetFullPath($BinaryDirectory).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
Assert-True ($Root.StartsWith($Boundary, [StringComparison]::OrdinalIgnoreCase)) 'Fixture must stay within the binary directory.'
try {
    $Game = Join-Path $Root 'game'
    $RunId = 'host-validation-result'
    $RunDirectory = Join-Path $Game ".cojvr-evidence/runs/$RunId"
    New-Item -ItemType Directory -Path $RunDirectory -Force | Out-Null
    $Build = [ordered]@{
        manifestId = ('A' * 64)
        source = [ordered]@{ headCommit = ('b' * 40); dirty = $true; trackedDiffSha256 = ('C' * 64) }
        build = [ordered]@{ configuration = 'Release'; platform = 'Win32'; diagnosticMode = 'd3d9_native_stereo' }
    }
    $BuildPath = Join-Path $RunDirectory 'build-manifest.json'
    Write-Json $BuildPath $Build
    $Deployment = @(
        @{role='openvr_runtime'; destination='openvr_api.dll'},
        @{role='openvr_action_manifest'; destination='cojvr_openvr_input/actions.json'},
        @{role='openvr_binding_psvr2_sense'; destination='cojvr_openvr_input/bindings/psvr2_sense.json'}
    )
    foreach ($File in $Deployment) {
        $Path = Join-Path $Game $File.destination
        New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($Path)) -Force | Out-Null
        [IO.File]::WriteAllText($Path, 'synthetic host artifact')
        $File.sha256 = (Get-FileHash -LiteralPath $Path).Hash
    }
    $Run = [ordered]@{
        runId = $RunId; buildManifestId = $Build.manifestId
        buildManifestSha256 = (Get-FileHash -LiteralPath $BuildPath).Hash
        diagnosticMode = 'd3d9_native_stereo'; source = $Build.source
        game = @{
            executable = @{sha256='5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE'; knownExactBuild=$true}
            engine = @{sha256='DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8'; knownInspectedBuild=$true}
        }
        validation = @{profile='startup'; requirePerformanceSummary=$false}
        deployment = $Deployment
    }
    $RunPath = Join-Path $Game '.cojvr-run.json'
    Write-Json $RunPath $Run
    Write-Json (Join-Path $Game '.cojvr-d3d9-stage.json') @{
        runId=$RunId; buildManifestId=$Build.manifestId; buildManifestSha256=$Run.buildManifestSha256
        diagnosticMode='d3d9_native_stereo'
    }
    $Log = @(
        "run_start: run_id=$RunId build_manifest_id=$($Build.manifestId) pid=123",
        'native_stereo_d3d9_factory: status=d3d9ex_primary fallback=classic_create_device transport=d3d9ex_shared_texture_ring',
        'native_stereo_device: status=observed device_api=d3d9ex',
        'native_stereo_startup_event: event=factory_identity;device=host_fixture;matches=true',
        'native_stereo_startup_event: event=present_exit;frame_sequence=90;hr=0x0',
        'native_stereo_flat_theater: status=captured frame_sequence=1',
        'native_stereo_presenter_transition: status=content_mode mode=flat_theater',
        'native_stereo_startup_event: event=device_method_enter;device=host_fixture;method=Present',
        'native_stereo_startup_event: event=device_method_exit;device=host_fixture;method=Present',
        'native_stereo_factory_hook: status=restored',
        'native_stereo_device_hook: status=restored',
        'native_stereo_swapchain_hook: status=restored',
        'native_stereo_runtime: status=stopped',
        'native_stereo_pre_exit_hook: status=installed',
        'native_stereo_pre_exit: stage=begin boundary=CoJ.exe!DestroyGame',
        'native_stereo_pre_exit_hook: status=restored',
        'native_stereo_pre_exit: stage=end boundary=CoJ.exe!DestroyGame',
        "run_end: run_id=$RunId"
    )
    $LogPath = Join-Path $Game 'cojvr.log'
    Set-Content -LiteralPath $LogPath -Value $Log -Encoding utf8
    $Verify = Join-Path $SourceDirectory 'tools/verify_native_stereo_live_test.ps1'
    $Collect = Join-Path $SourceDirectory 'tools/collect_run_evidence.ps1'
    $ResultPath = Join-Path $RunDirectory 'analysis/validation-result.json'
    & $Verify -GameDirectory $Game
    Assert-True (Test-Path -LiteralPath $ResultPath) 'Canonical verifier did not persist its early-return PASS.'
    $Result = Get-Content -LiteralPath $ResultPath -Raw | ConvertFrom-Json
    Assert-True ($Result.outcome -eq 'pass' -and $Result.verifier.succeeded) 'Startup canonical PASS was lost.'
    Assert-True ($Result.runId -eq $RunId -and $Result.buildManifestId -eq $Build.manifestId -and
        $Result.source.headCommit -eq $Build.source.headCommit -and $Result.validationProfile -eq 'startup') 'Verdict identity was lost.'
    Assert-True ($Result.headsetAcceptance -eq 'not_evaluated') 'Telemetry must not accept a headset gate.'

    # A substring in another record is not the correlated end of this run.
    Set-Content -LiteralPath $LogPath -Value @($Log -replace '^run_end:', 'unrelated_run_end:') -Encoding utf8
    Assert-Rejected { & $Verify -GameDirectory $Game } 'correlated runtime start/end'
    Set-Content -LiteralPath $LogPath -Value $Log -Encoding utf8
    & $Verify -GameDirectory $Game

    $OperatorPath = Join-Path $RunDirectory 'operator-observations.json'
    Write-Json $OperatorPath @{runId=$RunId; observations=@(@{gate='G1'; outcome='unavailable'; note='host fixture only'})}
    $Package = Join-Path $Root 'evidence.zip'
    & $Collect -GameDirectory $Game -OutputPath $Package
    $Evidence = Get-Content (Join-Path $RunDirectory 'evidence-manifest.json') -Raw | ConvertFrom-Json
    Assert-True ($Evidence.validation.outcome -eq 'pass') 'Collection lost the verifier outcome.'
    foreach ($Name in @('analysis/validation-result.json', 'build-manifest.json', 'operator-observations.json')) {
        $Entry = @($Evidence.files | Where-Object path -eq $Name)
        Assert-True ($Entry.Count -eq 1 -and $Entry[0].sha256 -eq (Get-FileHash (Join-Path $RunDirectory $Name)).Hash) "Missing hash inventory for $Name."
    }
    $Extracted = Join-Path $Root 'extracted'
    Expand-Archive -LiteralPath $Package -DestinationPath $Extracted
    Assert-True ((Get-FileHash (Join-Path $Extracted 'analysis/validation-result.json')).Hash -eq
        (Get-FileHash $ResultPath).Hash) 'ZIP changed the verifier result.'

    # A complete package and positive operator note cannot erase canonical rejection.
    Set-Content -LiteralPath $LogPath -Value @($Log | Where-Object {$_ -notmatch 'event=present_exit'}) -Encoding utf8
    Assert-Rejected { & $Verify -GameDirectory $Game } 'at least 90'
    $Result = Get-Content $ResultPath -Raw | ConvertFrom-Json
    Assert-True ($Result.outcome -eq 'fail' -and -not $Result.verifier.succeeded -and
        $Result.reasons[0].message -match 'at least 90') 'Canonical rejection/reason was lost.'
    Write-Json $OperatorPath @{runId=$RunId; observations=@(@{gate='startup'; outcome='accepted'})}
    & $Collect -GameDirectory $Game -OutputPath $Package
    $Evidence = Get-Content (Join-Path $RunDirectory 'evidence-manifest.json') -Raw | ConvertFrom-Json
    Assert-True (-not $Evidence.incomplete -and $Evidence.validation.outcome -eq 'fail') 'Complete collection promoted a failed verifier.'

    # A changed log/profile/build/deployed artifact invalidates an old PASS.
    Set-Content -LiteralPath $LogPath -Value $Log -Encoding utf8
    Assert-Rejected { & $Collect -GameDirectory $Game -OutputPath $Package } 'validation result.*inputs'
    & $Verify -GameDirectory $Game
    $Run.validation.profile = 'transport'
    Write-Json $RunPath $Run
    Assert-Rejected { & $Collect -GameDirectory $Game -OutputPath $Package } 'validation result.*inputs'
    $Run.validation.profile = 'startup'
    Write-Json $RunPath $Run
    [IO.File]::WriteAllText((Join-Path $Game 'openvr_api.dll'), 'changed')
    Assert-Rejected { & $Collect -GameDirectory $Game -OutputPath $Package } 'validation result.*inputs'
    Assert-Rejected { & $Verify -GameDirectory $Game } 'deployed file changed'
    & $Collect -GameDirectory $Game -OutputPath $Package
    $Evidence = Get-Content (Join-Path $RunDirectory 'evidence-manifest.json') -Raw | ConvertFrom-Json
    Assert-True ($Evidence.validation.outcome -eq 'fail') 'Provenance rejection was not collected.'
    [IO.File]::WriteAllText((Join-Path $Game 'openvr_api.dll'), 'synthetic host artifact')

    Set-Content -LiteralPath $LogPath -Value @($Log | Where-Object {$_ -notmatch '^run_end:'}) -Encoding utf8
    Assert-Rejected { & $Verify -GameDirectory $Game } 'normal proxy finalization'
    & $Collect -GameDirectory $Game -OutputPath $Package
    $Evidence = Get-Content (Join-Path $RunDirectory 'evidence-manifest.json') -Raw | ConvertFrom-Json
    Assert-True ($Evidence.incomplete -and $Evidence.validation.outcome -ne 'pass') 'Partial run became a pass.'

    Remove-Item -LiteralPath $ResultPath
    Set-Content -LiteralPath $LogPath -Value $Log -Encoding utf8
    & $Collect -GameDirectory $Game -OutputPath $Package
    $Result = Get-Content $ResultPath -Raw | ConvertFrom-Json
    Assert-True ($Result.outcome -eq 'inconclusive' -and -not $Result.verifier.executed -and
        $Result.reasons[0].code -eq 'verifier_not_executed') 'Missing verifier became acceptance.'
    Add-Content -LiteralPath $LogPath -Value 'late diagnostic observation' -Encoding utf8
    & $Collect -GameDirectory $Game -OutputPath $Package
    $Result = Get-Content $ResultPath -Raw | ConvertFrom-Json
    Assert-True ($Result.outcome -eq 'inconclusive' -and -not $Result.verifier.executed) 'Repeated collection invented a verifier execution.'

    # Exercise the real finish entry point in an isolated workflow. A negative
    # verifier must survive successful evidence collection AND original restore.
    $Workflow = Join-Path $Root 'workflow'
    New-Item -ItemType Directory -Path $Workflow -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $SourceDirectory 'tools') -Destination $Workflow -Recurse
    $Proxy = Join-Path $Game 'd3d9.dll'
    $Backup = Join-Path $Game 'd3d9.cojvr-backup.dll'
    [IO.File]::WriteAllText($Proxy, 'staged host proxy')
    [IO.File]::WriteAllText($Backup, 'original host proxy')
    $OriginalHash = (Get-FileHash $Backup).Hash
    $ProxyHash = (Get-FileHash $Proxy).Hash
    $Run.deployment += @{role='proxy'; destination='d3d9.dll'; sha256=$ProxyHash}
    Write-Json $RunPath $Run
    Write-Json (Join-Path $Game '.cojvr-d3d9-stage.json') @{
        runId=$RunId; buildManifestId=$Build.manifestId; buildManifestSha256=$Run.buildManifestSha256
        diagnosticMode='d3d9_native_stereo'; hadOriginalD3D9=$true
        originalProxySha256=$OriginalHash; stagedProxySha256=$ProxyHash
    }
    Set-Content -LiteralPath $LogPath -Value @($Log | Where-Object {$_ -notmatch 'event=present_exit'}) -Encoding utf8
    Assert-Rejected { & (Join-Path $Workflow 'tools/vr_test.ps1') finish -GameDirectory $Game } 'live verifier failed:.*at least 90'
    Assert-True ((Get-FileHash $Proxy).Hash -eq $OriginalHash -and
        -not (Test-Path (Join-Path $Game '.cojvr-d3d9-stage.json'))) 'Failed verifier prevented transactional original restoration.'
    $Result = Get-Content $ResultPath -Raw | ConvertFrom-Json
    $Evidence = Get-Content (Join-Path $RunDirectory 'evidence-manifest.json') -Raw | ConvertFrom-Json
    Assert-True ($Result.outcome -eq 'fail' -and $Evidence.validation.outcome -eq 'fail' -and -not $Evidence.incomplete) 'finish promoted a complete restored rejection.'
    $FinishZip = Join-Path $Game ".cojvr-evidence/packages/$RunId.zip"
    $FinishExtracted = Join-Path $Root 'finish-extracted'
    Expand-Archive -LiteralPath $FinishZip -DestinationPath $FinishExtracted
    Assert-True ((Get-FileHash (Join-Path $FinishExtracted 'analysis/validation-result.json')).Hash -eq
        (Get-FileHash $ResultPath).Hash) 'finish ZIP lost the failed result.'

    Copy-Item -LiteralPath $Proxy -Destination $Backup
    [IO.File]::WriteAllText($Proxy, 'staged host proxy')
    Write-Json (Join-Path $Game '.cojvr-d3d9-stage.json') @{
        runId=$RunId; buildManifestId=$Build.manifestId; buildManifestSha256=$Run.buildManifestSha256
        diagnosticMode='d3d9_native_stereo'; hadOriginalD3D9=$true
        originalProxySha256=$OriginalHash; stagedProxySha256=$ProxyHash
    }
    $PreviousFaultTest = $env:COJVR_DEPLOYMENT_FAILURE_TEST
    $PreviousFaultPoint = $env:COJVR_DEPLOYMENT_FAIL_AFTER
    try {
        $env:COJVR_DEPLOYMENT_FAILURE_TEST = '1'
        $env:COJVR_DEPLOYMENT_FAIL_AFTER = 'unstage_proxy_removed'
        Assert-Rejected { & (Join-Path $Workflow 'tools/vr_test.ps1') finish -GameDirectory $Game } 'could not be restored'
    } finally {
        $env:COJVR_DEPLOYMENT_FAILURE_TEST = $PreviousFaultTest
        $env:COJVR_DEPLOYMENT_FAIL_AFTER = $PreviousFaultPoint
    }
    $ResultBeforeRetry = (Get-FileHash $ResultPath).Hash
    $ZipBeforeRetry = (Get-FileHash $FinishZip).Hash
    Assert-True (-not (Test-Path $Proxy) -and (Test-Path (Join-Path $Game '.cojvr-d3d9-stage.json'))) 'Fault did not interrupt after proxy removal.'
    $RetryError = $null
    try { & (Join-Path $Workflow 'tools/vr_test.ps1') finish -GameDirectory $Game } catch { $RetryError = $_.Exception.Message }
    Assert-True ($null -eq $RetryError) "Restoration-only retry incorrectly revalidated retired inputs: $RetryError"
    Assert-True ((Get-FileHash $ResultPath).Hash -eq $ResultBeforeRetry -and
        (Get-FileHash $FinishZip).Hash -eq $ZipBeforeRetry) 'Restoration retry overwrote the original canonical verdict/package.'
    Assert-True ((Get-FileHash $Proxy).Hash -eq $OriginalHash -and
        -not (Test-Path (Join-Path $Game '.cojvr-d3d9-stage.json'))) 'Restoration retry did not recover the original.'

    # Test-only publication fault in the copied helper: finally/collection may
    # fail, but the real finish must still restore retained originals.
    $CopiedHelper = Join-Path $Workflow 'tools/coj_validation_result.ps1'
    $HelperSource = Get-Content -LiteralPath $CopiedHelper -Raw
    $Publication = '[IO.File]::Move($Temporary, $Context.ResultPath, $true)'
    Assert-True ($HelperSource.Contains($Publication)) 'Could not inject the fixture publication fault.'
    [IO.File]::WriteAllText($CopiedHelper, $HelperSource.Replace($Publication,
        "throw 'Injected validation result publication failure'"))
    Copy-Item -LiteralPath $Proxy -Destination $Backup
    [IO.File]::WriteAllText($Proxy, 'staged host proxy')
    Write-Json (Join-Path $Game '.cojvr-d3d9-stage.json') @{
        runId=$RunId; buildManifestId=$Build.manifestId; buildManifestSha256=$Run.buildManifestSha256
        diagnosticMode='d3d9_native_stereo'; hadOriginalD3D9=$true
        originalProxySha256=$OriginalHash; stagedProxySha256=$ProxyHash
    }
    Assert-Rejected { & (Join-Path $Workflow 'tools/vr_test.ps1') finish -GameDirectory $Game } 'evidence collection failed:.*publication failure'
    Assert-True ((Get-FileHash $Proxy).Hash -eq $OriginalHash -and
        -not (Test-Path (Join-Path $Game '.cojvr-d3d9-stage.json'))) 'Result publication failure prevented original restoration.'
    Assert-True (-not (Test-Path -LiteralPath $ResultPath)) 'Publication failure retained an earlier verifier result.'
    Write-Host 'PASS - persisted canonical pass/fail/inconclusive, identity, stale rejection, inventory and operator separation.'
} finally {
    if (Test-Path -LiteralPath $Root) { Remove-Item -LiteralPath $Root -Recurse -Force }
}
