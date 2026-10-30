param([string]$SourceDirectory = (Join-Path $PSScriptRoot '..'),
      [string]$BinaryDirectory = (Join-Path $PSScriptRoot '../work/neural-deployment-tests'),
      [switch]$UnstageCompatibility)
$ErrorActionPreference = 'Stop'
$SourceDirectory = [IO.Path]::GetFullPath($SourceDirectory)
$Scratch = Join-Path ([IO.Path]::GetFullPath($BinaryDirectory)) ([guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($Scratch)
. (Join-Path $SourceDirectory 'tools/deployment_transaction.ps1')
$Tokens = $null; $Errors = $null
$Ast = [Management.Automation.Language.Parser]::ParseFile(
    (Join-Path $SourceDirectory 'tools/stage_d3d9_proxy.ps1'), [ref]$Tokens, [ref]$Errors)
if ($Errors.Count) { throw ($Errors -join "`n") }
# Execute the production Neural boundaries without weakening exact-game SHA
# admission or invoking prepare/stage on synthetic or installed game files.
foreach ($Name in @('Get-PeMachine', 'New-CojvrNeuralCandidate',
    'New-CojvrNeuralConfigAsset', 'Install-CojvrNeuralCandidate')) {
    $Function = $Ast.Find({param($Node)
        $Node -is [Management.Automation.Language.FunctionDefinitionAst] -and $Node.Name -eq $Name
    }, $true)
    if ($null -eq $Function) { throw "Missing production boundary: $Name" }
    Invoke-Expression $Function.Extent.Text
}
function Assert-True([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Assert-Rejected([scriptblock]$Command) {
    $Rejected = $false
    try { & $Command | Out-Null } catch { $Rejected = $true }
    Assert-True $Rejected 'Expected fail-closed rejection.'
}
function Write-Pe([string]$Path, [uint16]$Machine = 0x8664) {
    $Bytes = [byte[]]::new(128)
    $Bytes[0] = 0x4d; $Bytes[1] = 0x5a; $Bytes[0x3c] = 0x40
    $Bytes[0x40] = 0x50; $Bytes[0x41] = 0x45
    [BitConverter]::GetBytes($Machine).CopyTo($Bytes, 0x44)
    [IO.File]::WriteAllBytes($Path, $Bytes)
}
function New-Fixture {
    $Game = Join-Path $Scratch ([guid]::NewGuid().ToString('N'))
    [void][IO.Directory]::CreateDirectory($Game)
    return $Game
}
function New-NormalStagedFixture([bool]$HadOriginal) {
    $Game = New-Fixture
    $Config = Join-Path $Game 'cojvr-neural.ini'
    if ($HadOriginal) { [IO.File]::WriteAllText($Config, 'original-user-config') }
    $Asset = New-CojvrNeuralConfigAsset $Game $Candidate
    $RunId = [guid]::NewGuid().ToString('N')
    $RunDirectory = Join-Path $Game ('.cojvr-evidence/runs/' + $RunId)
    [void][IO.Directory]::CreateDirectory($RunDirectory)
    [IO.File]::WriteAllText((Join-Path $Game 'd3d9.dll'), 'staged fixture proxy')
    [void](Write-CojvrDeploymentJournal $Game 'stage' $RunId 'd3d9_native_stereo' @($Asset))
    Install-CojvrNeuralCandidate $Game $RunDirectory $RunId $Candidate $Asset
    $Stage = @{
        schemaVersion=2; runId=$RunId; diagnosticMode='d3d9_native_stereo'
        hadOriginalD3D9=$false; cameraControlManaged=$false; openVrRuntimeManaged=$false; openVrInputManaged=$false
        stagedProxySha256=(Get-CojvrFileSha256 (Join-Path $Game 'd3d9.dll'))
        neuralConfigAsset=$Asset; neuralCandidate=$Candidate.evidence
    }
    $Stage | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $Game '.cojvr-d3d9-stage.json') -Encoding UTF8
    Remove-Item -LiteralPath (Join-Path $Game '.cojvr-deployment-transaction.json')
    return $Game
}
$Runtime = Join-Path $Scratch ('runtime espacio ' + [char]0x00f1)
[void][IO.Directory]::CreateDirectory($Runtime)
$Helper = Join-Path $Scratch ('helper ' + [char]0x00f1 + '.exe')
Write-Pe $Helper
Write-Pe (Join-Path $Runtime 'nvngx_dlssnr.dll')
Write-Pe (Join-Path $Runtime 'nvngx.dll_nr_bridge.dll')
$PreviousTest = $env:COJVR_DEPLOYMENT_FAILURE_TEST
$PreviousCheckpoint = $env:COJVR_DEPLOYMENT_FAIL_AFTER
try {
    Assert-True ($null -eq (New-CojvrNeuralCandidate '' '')) 'Absent opt-in created a candidate.'
    Assert-Rejected { New-CojvrNeuralCandidate $Helper '' }
    Assert-Rejected { New-CojvrNeuralCandidate '' $Runtime }
    foreach ($Bad in @('relative.exe', 'C:relative.exe', '\\server\share\helper.exe',
        ($Helper + "`r`nEnabled=1"), ($Helper + ';comment'), ('"' + $Helper + '"'))) {
        Assert-Rejected { New-CojvrNeuralCandidate $Bad $Runtime }
    }
    Write-Pe $Helper 0x014c
    Assert-Rejected { New-CojvrNeuralCandidate $Helper $Runtime }
    Write-Pe $Helper
    foreach ($Name in @('nvngx_dlssnr.dll', 'nvngx.dll_nr_bridge.dll')) {
        $File = Join-Path $Runtime $Name
        Move-Item -LiteralPath $File -Destination ($File + '.saved')
        Assert-Rejected { New-CojvrNeuralCandidate $Helper $Runtime }
        Move-Item -LiteralPath ($File + '.saved') -Destination $File
    }
    $Candidate = New-CojvrNeuralCandidate $Helper $Runtime
    Assert-True ($Candidate.evidence.dependencies.Count -eq 3) 'Incomplete external identity inventory.'
    foreach ($Dependency in $Candidate.evidence.dependencies) {
        Assert-True ($Dependency.sha256 -eq (Get-CojvrFileSha256 $Dependency.path)) 'Wrong external SHA-256.'
    }
    $Game = New-Fixture
    Assert-True ($null -eq (New-CojvrNeuralConfigAsset $Game $null)) 'Default-off path created an asset.'
    foreach ($Name in @('cojvr-neural.ini', 'cojvr-neural.cojvr-backup.ini', 'cojvr-neural.cojvr-installing.ini')) {
        $File = Join-Path $Game $Name
        [IO.File]::WriteAllText($File, 'unrecorded-user-file')
        if ($Name -eq 'cojvr-neural.ini') { Assert-Rejected { New-CojvrNeuralConfigAsset $Game $null } }
        else { Assert-Rejected { New-CojvrNeuralConfigAsset $Game $Candidate } }
        Assert-True ([IO.File]::ReadAllText($File) -eq 'unrecorded-user-file') 'Rejected inventory changed a user file.'
        Remove-Item -LiteralPath $File
    }
    # A dependency changed after admission must not move an original config.
    $Config = Join-Path $Game 'cojvr-neural.ini'
    [IO.File]::WriteAllText($Config, 'original-user-config')
    $Asset = New-CojvrNeuralConfigAsset $Game $Candidate
    $RunId = [guid]::NewGuid().ToString('N')
    $RunDirectory = Join-Path $Game ('.cojvr-evidence/runs/' + $RunId)
    [void][IO.Directory]::CreateDirectory($RunDirectory)
    Assert-Rejected { Install-CojvrNeuralCandidate $Game $RunDirectory $RunId $Candidate $Asset }
    [void](Write-CojvrDeploymentJournal $Game 'stage' $RunId 'd3d9_native_stereo' @($Asset))
    [IO.File]::AppendAllText($Helper, 'changed dependency')
    Assert-Rejected { Install-CojvrNeuralCandidate $Game $RunDirectory $RunId $Candidate $Asset }
    Assert-True ([IO.File]::ReadAllText($Config) -eq 'original-user-config') 'Dependency rejection moved original config.'
    Assert-True (-not (Test-Path (Join-Path $RunDirectory 'neural-dependencies.json'))) 'Rejected dependencies published evidence.'
    Write-Pe $Helper
    [void](Complete-CojvrDeploymentRecovery $Game)
    foreach ($HadOriginal in @($false, $true)) {
        foreach ($Checkpoint in @('', 'stage_neural_backup_created', 'stage_neural_published')) {
            if (-not $HadOriginal -and $Checkpoint -eq 'stage_neural_backup_created') { continue }
            $Game = Join-Path $Scratch ([guid]::NewGuid().ToString('N'))
            [void][IO.Directory]::CreateDirectory($Game)
            $Config = Join-Path $Game 'cojvr-neural.ini'
            $Original = [byte[]](0, 1, 2, 255, 10)
            if ($HadOriginal) { [IO.File]::WriteAllBytes($Config, $Original) }
            $Asset = New-CojvrNeuralConfigAsset $Game $Candidate
            $RunId = [guid]::NewGuid().ToString('N')
            $RunDirectory = Join-Path $Game ('.cojvr-evidence/runs/' + $RunId)
            [void][IO.Directory]::CreateDirectory($RunDirectory)
            [void](Write-CojvrDeploymentJournal $Game 'stage' $RunId 'd3d9_native_stereo' @($Asset))
            $Journal = Get-Content (Join-Path $Game '.cojvr-deployment-transaction.json') -Raw | ConvertFrom-Json
            Assert-True ($Journal.assets.Count -eq 1 -and $Journal.assets[0].destination -eq 'cojvr-neural.ini') 'Config missing from real journal.'
            $env:COJVR_DEPLOYMENT_FAILURE_TEST = '1'
            $env:COJVR_DEPLOYMENT_FAIL_AFTER = $Checkpoint
            if ($Checkpoint) {
                Assert-Rejected { Install-CojvrNeuralCandidate $Game $RunDirectory $RunId $Candidate $Asset }
            } else {
                Install-CojvrNeuralCandidate $Game $RunDirectory $RunId $Candidate $Asset
                $Bytes = [IO.File]::ReadAllBytes($Config)
                Assert-True ($Bytes[0] -eq 255 -and $Bytes[1] -eq 254) 'INI lacks UTF16LE BOM.'
                $Text = [IO.File]::ReadAllText($Config)
                Assert-True ($Text -eq "[Neural]`r`nEnabled=1`r`nHostPath=$Helper`r`nRuntimeDirectory=$Runtime`r`nStageHeight=384`r`nTimeoutMs=8`r`n") 'INI values/Unicode paths changed.'
                $Evidence = Get-Content (Join-Path $RunDirectory 'neural-dependencies.json') -Raw | ConvertFrom-Json
                Assert-True ($Evidence.runId -eq $RunId -and $Evidence.dependencies.Count -eq 3) 'Evidence lost run/external identity.'
                Assert-True (-not (Test-Path (Join-Path $Game 'nvngx_dlssnr.dll'))) 'Model copied into game.'
            }
            $env:COJVR_DEPLOYMENT_FAIL_AFTER = ''
            if (-not $Checkpoint) {
                # The official unstage entry point dispatches pending stage journals
                # through real generic recovery, without role allow-list changes.
                & (Join-Path $SourceDirectory 'tools/unstage_d3d9_proxy.ps1') -GameDirectory $Game | Out-Null
            } else { Assert-True (Complete-CojvrDeploymentRecovery $Game) 'Config transaction did not recover.' }
            if ($HadOriginal) {
                Assert-True ([Convert]::ToBase64String([IO.File]::ReadAllBytes($Config)) -eq [Convert]::ToBase64String($Original)) 'Original config was not restored byte-for-byte.'
            } else { Assert-True (-not (Test-Path -LiteralPath $Config)) 'Generated config survived recovery.' }
            Assert-True (-not (Complete-CojvrDeploymentRecovery $Game)) 'Recovery was not idempotent.'
        }
    }
    foreach ($Tamper in @('cojvr-neural.ini', 'cojvr-neural.cojvr-backup.ini')) {
        $Game = New-Fixture
        $Config = Join-Path $Game 'cojvr-neural.ini'
        [IO.File]::WriteAllText($Config, 'original-user-config')
        $Asset = New-CojvrNeuralConfigAsset $Game $Candidate
        $RunId = [guid]::NewGuid().ToString('N')
        $RunDirectory = Join-Path $Game ('.cojvr-evidence/runs/' + $RunId)
        [void][IO.Directory]::CreateDirectory($RunDirectory)
        [void](Write-CojvrDeploymentJournal $Game 'stage' $RunId 'd3d9_native_stereo' @($Asset))
        Install-CojvrNeuralCandidate $Game $RunDirectory $RunId $Candidate $Asset
        [IO.File]::AppendAllText((Join-Path $Game $Tamper), 'external edit')
        $Before = @(Get-CojvrDirectoryManifest $Game)
        Assert-Rejected { Complete-CojvrDeploymentRecovery $Game }
        Assert-True (Test-CojvrDirectoryMatchesManifest $Game $Before) 'Recovery changed assets before rejecting a foreign config/backup.'
    }
    if ($UnstageCompatibility) {
        foreach ($HadOriginal in @($false, $true)) {
            $Game = New-NormalStagedFixture $HadOriginal
            $Config = Join-Path $Game 'cojvr-neural.ini'
            & (Join-Path $SourceDirectory 'tools/unstage_d3d9_proxy.ps1') -GameDirectory $Game | Out-Null
            if ($HadOriginal) {
                Assert-True ([IO.File]::ReadAllText($Config) -eq 'original-user-config') 'Official normal unstage did not restore Neural original.'
            } else { Assert-True (-not (Test-Path -LiteralPath $Config)) 'Official normal unstage omitted Neural config.' }
        }
        foreach ($Checkpoint in @('unstage_neural_removed', 'unstage_neural_original_restored')) {
            $Game = New-NormalStagedFixture $true
            $env:COJVR_DEPLOYMENT_FAIL_AFTER = $Checkpoint
            Assert-Rejected { & (Join-Path $SourceDirectory 'tools/unstage_d3d9_proxy.ps1') -GameDirectory $Game }
            $Journal = Get-Content (Join-Path $Game '.cojvr-deployment-transaction.json') -Raw | ConvertFrom-Json
            Assert-True (@($Journal.assets | Where-Object { $_.role -eq 'neural_config' -and $_.kind -eq 'file' }).Count -eq 1) 'Unstage journal omitted Neural restore inventory.'
            $env:COJVR_DEPLOYMENT_FAIL_AFTER = ''
            & (Join-Path $SourceDirectory 'tools/unstage_d3d9_proxy.ps1') -GameDirectory $Game | Out-Null
            Assert-True ([IO.File]::ReadAllText((Join-Path $Game 'cojvr-neural.ini')) -eq 'original-user-config') 'Interrupted normal unstage lost original config.'
        }
        foreach ($Case in @('role', 'kind', 'destination', 'backup', 'temporary', 'hadOriginal',
            'stagedHash', 'originalHash', 'configHash', 'runId', 'missingAsset', 'missingCandidate',
            'tamperedConfig', 'tamperedBackup', 'missingBackup', 'foreignTemporary', 'foreignBackup')) {
            $Game = New-NormalStagedFixture ($Case -ne 'foreignBackup')
            $StatePath = Join-Path $Game '.cojvr-d3d9-stage.json'
            $Stage = Get-Content -LiteralPath $StatePath -Raw | ConvertFrom-Json
            switch ($Case) {
                'role' { $Stage.neuralConfigAsset.role = 'proxy' }
                'kind' { $Stage.neuralConfigAsset.kind = 'directory' }
                'destination' { $Stage.neuralConfigAsset.destination = '../outside-user-file.txt' }
                'backup' { $Stage.neuralConfigAsset.backup = '../outside-user-file.txt' }
                'temporary' { $Stage.neuralConfigAsset.temporary = '../outside-user-file.txt' }
                'hadOriginal' { $Stage.neuralConfigAsset.hadOriginal = 'true' }
                'stagedHash' { $Stage.neuralConfigAsset.stagedSha256 = 'not-a-hash' }
                'originalHash' { $Stage.neuralConfigAsset.originalSha256 = 'not-a-hash' }
                'configHash' { $Stage.neuralCandidate.configSha256 = ('0' * 64) }
                'runId' { $Stage.neuralCandidate.runId = 'another-run' }
                'missingAsset' { $Stage.neuralConfigAsset = $null }
                'missingCandidate' { $Stage.neuralCandidate = $null }
                'tamperedConfig' { [IO.File]::AppendAllText((Join-Path $Game 'cojvr-neural.ini'), 'external edit') }
                'tamperedBackup' { [IO.File]::AppendAllText((Join-Path $Game 'cojvr-neural.cojvr-backup.ini'), 'external edit') }
                'missingBackup' { Remove-Item -LiteralPath (Join-Path $Game 'cojvr-neural.cojvr-backup.ini') }
                'foreignTemporary' { [IO.File]::WriteAllText((Join-Path $Game 'cojvr-neural.cojvr-installing.ini'), 'external temporary') }
                'foreignBackup' { [IO.File]::WriteAllText((Join-Path $Game 'cojvr-neural.cojvr-backup.ini'), 'external backup') }
            }
            $Stage | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $StatePath -Encoding UTF8
            $Before = @(Get-CojvrDirectoryManifest $Game)
            Assert-Rejected { & (Join-Path $SourceDirectory 'tools/unstage_d3d9_proxy.ps1') -GameDirectory $Game }
            Assert-True (Test-CojvrDirectoryMatchesManifest $Game $Before) "Normal unstage mutated inventory before rejecting $Case."
        }
        Write-Host 'PASS: official normal unstage restores/removes Neural config.'
        Write-Host 'PASS: normal unstage rejects 17 altered inventories without mutation; interrupted restoration recovers.'
    }
    Write-Host 'PASS: Neural validation, UTF16 config, identities, journal and interrupted recovery.'
} finally {
    $env:COJVR_DEPLOYMENT_FAILURE_TEST = $PreviousTest
    $env:COJVR_DEPLOYMENT_FAIL_AFTER = $PreviousCheckpoint
    Write-Host "Neural deployment fixtures: $Scratch"
}
