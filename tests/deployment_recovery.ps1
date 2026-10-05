param(
    [Parameter(Mandatory = $true)] [string]$SourceDirectory,
    [Parameter(Mandatory = $true)] [string]$BinaryDirectory,
    [ValidateSet('All', 'Backups', 'Finish')] [string]$Case = 'All'
)
$ErrorActionPreference = 'Stop'
$BinaryRoot = [IO.Path]::GetFullPath($BinaryDirectory).TrimEnd('\') + '\'
$TestRoot = Join-Path $BinaryRoot ('deployment-recovery-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $TestRoot -Force | Out-Null
. (Join-Path $SourceDirectory 'tools/deployment_transaction.ps1')
. (Join-Path $SourceDirectory 'tools/coj_video_profile.ps1')
$Failures = @()
function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Write-Json([string]$Path, [object]$Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
}
function New-StagedFixture([string]$Name) {
    $Game = Join-Path $TestRoot $Name
    New-Item -ItemType Directory -Path $Game | Out-Null
    $Stage = [ordered]@{
        schemaVersion = 2; runId = 'synthetic-recovery'; diagnosticMode = 'd3d9_native_stereo'
        hadOriginalD3D9 = $true; cameraControlManaged = $true; hadOriginalCameraControl = $true
        openVrRuntimeManaged = $true; hadOriginalOpenVr = $true
        openVrInputManaged = $true; hadOriginalOpenVrInput = $true
    }
    foreach ($Entry in @(
        @('d3d9.dll', 'd3d9.cojvr-backup.dll', 'originalProxySha256', 'stagedProxySha256'),
        @('openvr_api.dll', 'openvr_api.cojvr-backup.dll', 'originalOpenVrSha256', 'stagedOpenVrSha256'),
        @('cojvr-camera-control.json', 'cojvr-camera-control.cojvr-backup.json', 'originalCameraControlSha256', 'unusedCameraHash')
    )) {
        [IO.File]::WriteAllText((Join-Path $Game $Entry[1]), 'original-' + $Entry[0])
        [IO.File]::WriteAllText((Join-Path $Game $Entry[0]), 'staged-' + $Entry[0])
        $Stage[$Entry[2]] = Get-CojvrFileSha256 (Join-Path $Game $Entry[1])
        $Stage[$Entry[3]] = Get-CojvrFileSha256 (Join-Path $Game $Entry[0])
    }
    $Input = Join-Path $Game 'cojvr_openvr_input'
    $Backup = Join-Path $Game 'cojvr_openvr_input.cojvr-backup'
    New-Item -ItemType Directory -Path (Join-Path $Input 'bindings'), $Backup | Out-Null
    [IO.File]::WriteAllText((Join-Path $Backup 'custom.json'), 'original-input')
    [IO.File]::WriteAllText((Join-Path $Input 'actions.json'), 'staged-actions')
    [IO.File]::WriteAllText((Join-Path $Input 'bindings/psvr2_sense.json'), 'staged-binding')
    $Stage.originalOpenVrInputManifest = @(Get-CojvrDirectoryManifest $Backup)
    $Stage.stagedOpenVrActionManifestSha256 = Get-CojvrFileSha256 (Join-Path $Input 'actions.json')
    $Stage.stagedOpenVrSenseBindingSha256 = Get-CojvrFileSha256 (Join-Path $Input 'bindings/psvr2_sense.json')
    Write-Json (Join-Path $Game '.cojvr-d3d9-stage.json') $Stage
    return $Game
}
function Invoke-Case([string]$CaseLabel, [scriptblock]$Operation) {
    try { & $Operation; Write-Host "PASS - $CaseLabel" } catch {
        $script:Failures += "$CaseLabel : $($_.Exception.Message)"
        Write-Host "FAIL - $CaseLabel : $($_.Exception.Message)"
    }
}
function Assert-RejectedWithoutMutation([string]$Game) {
    $Before = @(Get-CojvrDirectoryManifest $Game)
    $Rejected = $false
    try { & (Join-Path $SourceDirectory 'tools/unstage_d3d9_proxy.ps1') -GameDirectory $Game | Out-Null } catch { $Rejected = $true }
    Assert-True $Rejected 'Unstage accepted an altered or unprovable original backup.'
    Assert-True (Test-CojvrDirectoryMatchesManifest $Game $Before) 'Rejected unstage modified destinations, backups or journals.'
}
try {
    if ($Case -in @('All', 'Backups')) {
        foreach ($JournalOperation in @('stage','unstage')) {
            Invoke-Case "pending $JournalOperation journal rejects late altered executable before any mutation" {
                $Game = Join-Path $TestRoot ('journal-preflight-' + $JournalOperation)
                New-Item -ItemType Directory -Path $Game | Out-Null
                [IO.File]::WriteAllText((Join-Path $Game 'd3d9.dll'), 'staged-proxy')
                [IO.File]::WriteAllText((Join-Path $Game 'CoJ.exe'), 'staged-executable')
                [IO.File]::WriteAllText((Join-Path $Game 'CoJ.cojvr-backup.exe'), 'original-executable')
                $Assets = @(
                    @{role='proxy';kind='file';destination='d3d9.dll';backup='d3d9.cojvr-backup.dll';hadOriginal=$false;
                      originalSha256=$null;stagedSha256=(Get-CojvrFileSha256 (Join-Path $Game 'd3d9.dll'))},
                    @{role='game_executable';kind='file';destination='CoJ.exe';backup='CoJ.cojvr-backup.exe';hadOriginal=$true;
                      originalSha256=(Get-CojvrFileSha256 (Join-Path $Game 'CoJ.cojvr-backup.exe'));
                      stagedSha256=(Get-CojvrFileSha256 (Join-Path $Game 'CoJ.exe'))}
                )
                [void](Write-CojvrDeploymentJournal $Game $JournalOperation 'synthetic-journal' 'd3d9_native_stereo' $Assets)
                [IO.File]::AppendAllText((Join-Path $Game 'CoJ.cojvr-backup.exe'), 'tampered')
                $Before = @(Get-CojvrDirectoryManifest $Game)
                $Rejected = $false
                try { [void](Complete-CojvrDeploymentRecovery $Game) } catch { $Rejected = $true }
                Assert-True $Rejected 'Recovery accepted damaged executable backup.'
                Assert-True (Test-CojvrDirectoryMatchesManifest $Game $Before) 'Recovery mutated earlier assets before rejecting later executable.'
            }
        }
        Invoke-Case 'stage preserves empty input identity through JSON' {
            $Game = New-StagedFixture 'empty-input-serialization'
            $OpenVrInputDestination = Join-Path $Game 'cojvr_openvr_input'
            $OriginalInputManifest = @()
            $HadOriginalOpenVrInput = $true
            $IsNativeStereo = $true
            $IsOpenVrIntegration = $false
            $Destination = Join-Path $Game 'd3d9.dll'
            $BuildManifestPath = Join-Path $Game 'manifest.json'
            [IO.File]::WriteAllText($BuildManifestPath, '{}')
            $State = Join-Path $Game 'serialized-stage.json'
            $JournalAssets = @([ordered]@{role='proxy'; originalSha256=$null})
            # Execute only the real stage-state publication pipeline from the
            # parsed script. Exact game gating and deployment are never bypassed
            # to stage synthetic executables in this host fixture.
            $Tokens = $null; $Errors = $null
            $Ast = [Management.Automation.Language.Parser]::ParseFile(
                (Join-Path $SourceDirectory 'tools/stage_d3d9_proxy.ps1'), [ref]$Tokens, [ref]$Errors)
            $Publication = $Ast.Find({ param($Node)
                $Node -is [Management.Automation.Language.PipelineAst] -and
                $Node.Extent.Text.StartsWith('@{') -and
                $Node.Extent.Text.Contains('hadOriginalD3D9 = $HadOriginal')
            }, $true)
            Assert-True ($null -ne $Publication) 'Could not isolate stage-state publication.'
            Invoke-Expression $Publication.Extent.Text
            $Saved = Get-Content $State -Raw | ConvertFrom-Json
            Assert-True ($null -ne $Saved.originalOpenVrInputManifest -and @($Saved.originalOpenVrInputManifest).Count -eq 0) 'Stage erased the empty input identity during serialization.'
        }
        foreach ($Name in @('d3d9.cojvr-backup.dll', 'openvr_api.cojvr-backup.dll', 'cojvr-camera-control.cojvr-backup.json', 'cojvr_openvr_input.cojvr-backup/custom.json')) {
            Invoke-Case "altered $Name" {
                $Game = New-StagedFixture ('tampered-' + [guid]::NewGuid().ToString('N'))
                [IO.File]::AppendAllText((Join-Path $Game $Name), 'tampering')
                Assert-RejectedWithoutMutation $Game
            }
        }
        Invoke-Case 'unprovable legacy original' {
            $Game = New-StagedFixture 'legacy'
            $State = Join-Path $Game '.cojvr-d3d9-stage.json'
            $Stage = Get-Content $State -Raw | ConvertFrom-Json
            $Stage.PSObject.Properties.Remove('originalProxySha256')
            Write-Json $State $Stage
            Assert-RejectedWithoutMutation $Game
        }
        Invoke-Case 'valid originals and mutable control restore' {
            $Game = New-StagedFixture 'valid'
            $Stage = Get-Content (Join-Path $Game '.cojvr-d3d9-stage.json') -Raw | ConvertFrom-Json
            [IO.File]::WriteAllText((Join-Path $Game 'cojvr-camera-control.json'), '{"trackingEnabled":true}')
            & (Join-Path $SourceDirectory 'tools/unstage_d3d9_proxy.ps1') -GameDirectory $Game | Out-Null
            Assert-True ((Get-CojvrFileSha256 (Join-Path $Game 'd3d9.dll')) -eq $Stage.originalProxySha256) 'Proxy original not restored.'
            Assert-True ((Get-CojvrFileSha256 (Join-Path $Game 'openvr_api.dll')) -eq $Stage.originalOpenVrSha256) 'Runtime original not restored.'
            Assert-True ((Get-CojvrFileSha256 (Join-Path $Game 'cojvr-camera-control.json')) -eq $Stage.originalCameraControlSha256) 'Control original not restored.'
            Assert-True (Test-CojvrDirectoryMatchesManifest (Join-Path $Game 'cojvr_openvr_input') @($Stage.originalOpenVrInputManifest)) 'Input original not restored.'
            Assert-True (-not (Complete-CojvrDeploymentRecovery $Game)) 'Completed recovery was not idempotent.'
        }
        Invoke-Case 'empty original input directory restores' {
            $Game = New-StagedFixture 'empty-input'
            Remove-Item -LiteralPath (Join-Path $Game 'cojvr_openvr_input.cojvr-backup/custom.json')
            $State = Join-Path $Game '.cojvr-d3d9-stage.json'
            $Stage = Get-Content $State -Raw | ConvertFrom-Json
            $Stage.originalOpenVrInputManifest = @()
            Write-Json $State $Stage
            & (Join-Path $SourceDirectory 'tools/unstage_d3d9_proxy.ps1') -GameDirectory $Game | Out-Null
            Assert-True (Test-CojvrDirectoryMatchesManifest (Join-Path $Game 'cojvr_openvr_input') @()) 'Empty original input directory not restored.'
        }
    }
    if ($Case -in @('All', 'Finish')) {
        # Copy the real entry point and dependencies so its work/config paths are
        # isolated too. Never resolve Documents or invoke prepare in these tests.
        $WorkflowRoot = Join-Path $TestRoot 'workflow'
        New-Item -ItemType Directory -Path $WorkflowRoot | Out-Null
        Copy-Item -LiteralPath (Join-Path $SourceDirectory 'tools') -Destination $WorkflowRoot -Recurse
        $WorkflowWork = Join-Path $WorkflowRoot 'work'
        New-Item -ItemType Directory -Path $WorkflowWork | Out-Null
        $Finish = Join-Path $WorkflowRoot 'tools/vr_test.ps1'
        Invoke-Case 'finish rejects empty deployment inventory without mutation' {
            $Game = New-StagedFixture 'empty-journal'
            Remove-Item -LiteralPath (Join-Path $Game '.cojvr-d3d9-stage.json')
            [void](Write-CojvrDeploymentJournal $Game 'unstage' 'synthetic-recovery' 'd3d9_native_stereo' @())
            $Before = @(Get-CojvrDirectoryManifest $Game)
            $Rejected = $false
            try { & $Finish finish -GameDirectory $Game | Out-Null } catch { $Rejected = $true }
            Assert-True $Rejected 'Finish claimed recovery with an empty deployment inventory.'
            Assert-True (Test-CojvrDirectoryMatchesManifest $Game $Before) 'Rejected empty recovery changed evidence or assets.'
        }
        Invoke-Case 'finish retries video after staging removed' {
            $Game = New-StagedFixture 'video'
            & (Join-Path $SourceDirectory 'tools/unstage_d3d9_proxy.ps1') -GameDirectory $Game | Out-Null
            $Video = Join-Path $WorkflowWork 'Video.scr'
            $State = Join-Path $WorkflowWork 'vr-video-profile.json'
            $Backup = Join-Path $WorkflowWork 'vr-video-profile.backup'
            [IO.File]::WriteAllText($Video, "Resolution(2560,1440)`r`nFSAA(8)`r`n")
            $OriginalHash = Get-CojvrFileSha256 $Video
            [void](Apply-CoJVrVideoProfile $Video $State $Backup)
            $OriginalBytes = [IO.File]::ReadAllBytes($Backup)
            [IO.File]::AppendAllText($Backup, 'tampering')
            $Rejected = $false
            try { [void](Restore-CoJVrVideoProfile $State $Backup) } catch { $Rejected = $true }
            Assert-True $Rejected 'Corrupt video backup accepted.'
            Assert-True ((Test-Path $State) -and (Test-Path $Backup)) 'Failed video restore lost recovery evidence.'
            [IO.File]::WriteAllBytes($Backup, $OriginalBytes)
            & $Finish finish -GameDirectory $Game | Out-Null
            Assert-True ((Get-CojvrFileSha256 $Video) -eq $OriginalHash) 'Finish did not restore original video bytes.'
            Assert-True (-not (Test-Path $State) -and -not (Test-Path $Backup)) 'Finish left a video recovery journal.'
        }
        Invoke-Case 'finish retries deployment after staging removed' {
            $Game = New-StagedFixture 'deployment'
            $PreviousTest = $env:COJVR_DEPLOYMENT_FAILURE_TEST
            $PreviousCheckpoint = $env:COJVR_DEPLOYMENT_FAIL_AFTER
            try {
                $env:COJVR_DEPLOYMENT_FAILURE_TEST = '1'
                $env:COJVR_DEPLOYMENT_FAIL_AFTER = 'unstage_state_removed'
                $Rejected = $false
                try { & (Join-Path $SourceDirectory 'tools/unstage_d3d9_proxy.ps1') -GameDirectory $Game | Out-Null } catch { $Rejected = $_.Exception.Message -match 'Injected deployment failure' }
                Assert-True $Rejected 'Did not reach the interruption after stage removal.'
            } finally {
                $env:COJVR_DEPLOYMENT_FAILURE_TEST = $PreviousTest
                $env:COJVR_DEPLOYMENT_FAIL_AFTER = $PreviousCheckpoint
            }
            Assert-True (-not (Test-Path (Join-Path $Game '.cojvr-d3d9-stage.json'))) 'Stage still present at interruption.'
            Assert-True (Test-Path (Join-Path $Game '.cojvr-deployment-transaction.json')) 'Interrupted unstage lost journal.'
            $Before = @(Get-CojvrDirectoryManifest $Game | Where-Object path -ne '.cojvr-deployment-transaction.json')
            & $Finish finish -GameDirectory $Game | Out-Null
            Assert-True (Test-CojvrDirectoryMatchesManifest $Game $Before) 'Finish failed to clean journal or changed restored originals.'
            Assert-True (-not (Complete-CojvrDeploymentRecovery $Game)) 'Repeated recovery was not a no-op.'
        }
        Invoke-Case 'finish with no recovery refuses changes' {
            $Game = New-StagedFixture 'inactive'
            & (Join-Path $SourceDirectory 'tools/unstage_d3d9_proxy.ps1') -GameDirectory $Game | Out-Null
            $Before = @(Get-CojvrDirectoryManifest $Game)
            $Rejected = $false
            try { & $Finish finish -GameDirectory $Game | Out-Null } catch { $Rejected = $true }
            Assert-True $Rejected 'Finish accepted absent candidate/recovery as live validation.'
            Assert-True (Test-CojvrDirectoryMatchesManifest $Game $Before) 'Inactive finish modified originals.'
        }
    }
    if ($Failures.Count) { throw ($Failures -join [Environment]::NewLine) }
} finally {
    if (-not $Failures.Count -and ([IO.Path]::GetFullPath($TestRoot)).StartsWith($BinaryRoot, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $TestRoot -Recurse -Force
    } else { Write-Host "Retained failing fixtures: $TestRoot" }
}
