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
        # File-only inventories authorize files and their ancestor directories,
        # regardless of Hidden/System visibility. Foreign content must stop the
        # entire recovery before an earlier proxy or temporary is removed.
        foreach ($Location in @('destination', 'temporary', 'backup')) {
            foreach ($Shape in @('file', 'nested-file', 'empty-directory')) {
                foreach ($Flags in @([IO.FileAttributes]::Hidden, [IO.FileAttributes]::System,
                    ([IO.FileAttributes]::Hidden -bor [IO.FileAttributes]::System))) {
                    Invoke-Case "recovery preserves foreign $Shape ($Flags) in $Location" {
                        $Game = New-StagedFixture ('visibility-' + [guid]::NewGuid().ToString('N'))
                        $Input = Join-Path $Game 'cojvr_openvr_input'
                        $Backup = Join-Path $Game 'cojvr_openvr_input.cojvr-backup'
                        $Temporary = Join-Path $Game 'cojvr_openvr_input.cojvr-installing'
                        $Original = @(Get-CojvrDirectoryManifest $Backup)
                        $Staged = @(Get-CojvrDirectoryManifest $Input)
                        $Target = switch ($Location) {
                            'destination' { $Input }
                            'backup' { $Backup }
                            'temporary' {
                                [void][IO.Directory]::CreateDirectory($Temporary)
                                [IO.File]::WriteAllText((Join-Path $Temporary 'actions.json'), 'staged-actions')
                                $Temporary
                            }
                        }
                        $Assets = @(
                            @{role='proxy';kind='file';destination='d3d9.dll';hadOriginal=$false;
                              stagedSha256=(Get-CojvrFileSha256 (Join-Path $Game 'd3d9.dll'))},
                            @{role='openvr_input';kind='directory';destination='cojvr_openvr_input';
                              backup='cojvr_openvr_input.cojvr-backup';temporary='cojvr_openvr_input.cojvr-installing';
                              hadOriginal=$true;originalManifest=$Original;stagedManifest=$Staged}
                        )
                        [void](Write-CojvrDeploymentJournal $Game 'unstage' 'visibility-recovery' 'd3d9_native_stereo' $Assets)
                        $Foreign = Join-Path $Target 'foreign.txt'
                        if ($Shape -ne 'file') {
                            $Directory = Join-Path $Target 'foreign-directory'
                            [void][IO.Directory]::CreateDirectory($Directory)
                            $Foreign = if ($Shape -eq 'nested-file') { Join-Path $Directory 'foreign.txt' } else { $Directory }
                            [IO.File]::SetAttributes($Directory, ([IO.File]::GetAttributes($Directory) -bor $Flags))
                        }
                        if ($Shape -ne 'empty-directory') {
                            [IO.File]::WriteAllText($Foreign, 'external content must survive')
                            [IO.File]::SetAttributes($Foreign, ([IO.File]::GetAttributes($Foreign) -bor $Flags))
                        }
                        $Journal = Join-Path $Game '.cojvr-deployment-transaction.json'
                        $JournalHash = Get-CojvrFileSha256 $Journal
                        $ProxyHash = Get-CojvrFileSha256 (Join-Path $Game 'd3d9.dll')
                        $ForeignAttributes = [IO.File]::GetAttributes($Foreign)
                        $Rejected = $false
                        try { [void](Complete-CojvrDeploymentRecovery $Game) } catch { $Rejected = $true }
                        Assert-True $Rejected 'Recovery accepted foreign content omitted from its inventory.'
                        Assert-True ((Get-CojvrFileSha256 $Journal) -eq $JournalHash) 'Rejection lost/changed the journal.'
                        Assert-True ((Get-CojvrFileSha256 (Join-Path $Game 'd3d9.dll')) -eq $ProxyHash) 'Rejection removed an earlier asset.'
                        Assert-True ([IO.File]::GetAttributes($Foreign) -eq $ForeignAttributes) 'Rejection changed foreign visibility/data.'
                        if ($Shape -ne 'empty-directory') {
                            Assert-True ([IO.File]::ReadAllText($Foreign) -eq 'external content must survive') 'Rejection lost external bytes.'
                        }
                        Assert-True ([IO.Directory]::Exists($Backup)) 'Rejection lost original directory backup.'
                        if ($Location -eq 'temporary') {
                            Assert-True ([IO.File]::ReadAllText((Join-Path $Temporary 'actions.json')) -eq 'staged-actions') 'Rejection removed temporary content.'
                        }
                    }
                }
            }
        }
        Invoke-Case 'known hidden/system nested originals are inventoried and restore' {
            $Game = New-StagedFixture 'known-visibility'
            $Backup = Join-Path $Game 'cojvr_openvr_input.cojvr-backup'
            $HiddenDirectory = Join-Path $Backup 'private'
            [void][IO.Directory]::CreateDirectory($HiddenDirectory)
            $Known = Join-Path $HiddenDirectory 'settings.json'
            [IO.File]::WriteAllText($Known, 'original hidden system settings')
            $Flags = [IO.FileAttributes]::Hidden -bor [IO.FileAttributes]::System
            [IO.File]::SetAttributes($Known, $Flags)
            [IO.File]::SetAttributes($HiddenDirectory, ([IO.File]::GetAttributes($HiddenDirectory) -bor $Flags))
            $Original = @(Get-CojvrDirectoryManifest $Backup)
            Assert-True (@($Original | Where-Object { $_.path -eq 'private/settings.json' }).Count -eq 1) 'Original inventory omitted hidden/system content.'
            $State = Join-Path $Game '.cojvr-d3d9-stage.json'
            $Stage = Get-Content $State -Raw | ConvertFrom-Json
            $Stage.originalOpenVrInputManifest = $Original
            Write-Json $State $Stage
            & (Join-Path $SourceDirectory 'tools/unstage_d3d9_proxy.ps1') -GameDirectory $Game | Out-Null
            $Restored = Join-Path $Game 'cojvr_openvr_input/private/settings.json'
            Assert-True ([IO.File]::ReadAllText($Restored) -eq 'original hidden system settings') 'Known hidden/system original not restored.'
            Assert-True (([IO.File]::GetAttributes($Restored) -band $Flags) -eq $Flags) 'Restore changed original visibility flags.'
        }
        Invoke-Case 'known staged hidden/system nested files remain removable' {
            $Game = New-StagedFixture 'known-staged-visibility'
            $Input = Join-Path $Game 'cojvr_openvr_input'
            $Flags = [IO.FileAttributes]::Hidden -bor [IO.FileAttributes]::System
            [IO.File]::SetAttributes((Join-Path $Input 'bindings'), ([IO.File]::GetAttributes((Join-Path $Input 'bindings')) -bor $Flags))
            [IO.File]::SetAttributes((Join-Path $Input 'bindings/psvr2_sense.json'), $Flags)
            & (Join-Path $SourceDirectory 'tools/unstage_d3d9_proxy.ps1') -GameDirectory $Game | Out-Null
            Assert-True ([IO.File]::ReadAllText((Join-Path $Input 'custom.json')) -eq 'original-input') 'Known staged attributes prevented restoration.'
        }
        # Exercise the real native-input preflight/publication/deployment blocks
        # with synthetic files; never bypass the executable SHA-256 game gate.
        $Tokens = $null; $Errors = $null
        $StageAst = [Management.Automation.Language.Parser]::ParseFile(
            (Join-Path $SourceDirectory 'tools/stage_d3d9_proxy.ps1'), [ref]$Tokens, [ref]$Errors)
        $Preflight = $StageAst.Find({ param($Node)
            $Node -is [Management.Automation.Language.IfStatementAst] -and
            $Node.Extent.Text.StartsWith('if ($IsNativeStereo)') -and
            $Node.Extent.Text.Contains('$OpenVrInputSource =')
        }, $true)
        $Publish = $StageAst.Find({ param($Node)
            $Node -is [Management.Automation.Language.IfStatementAst] -and
            $Node.Extent.Text.StartsWith('if ($IsNativeStereo)') -and
            $Node.Extent.Text.Contains('stage_openvr_input_published')
        }, $true)
        $DeploymentBlock = $StageAst.Find({ param($Node)
            $Node -is [Management.Automation.Language.IfStatementAst] -and
            $Node.Extent.Text.StartsWith('if ($IsNativeStereo)') -and
            $Node.Extent.Text.Contains("destination = 'cojvr_openvr_input/'")
        }, $true)
        Assert-True ($Preflight -and $Publish -and $DeploymentBlock) 'Could not isolate real input deployment blocks.'
        $OriginalInputBlock = $StageAst.Find({ param($Node)
            $Node -is [Management.Automation.Language.IfStatementAst] -and
            $Node.Extent.Text.StartsWith('if ($IsNativeStereo)') -and
            $Node.Extent.Text.Contains('$OriginalInputManifest = @(if')
        }, $true)
        Assert-True ($null -ne $OriginalInputBlock) 'Could not isolate original input capture.'
        foreach ($OriginalShape in @('files', 'empty-root', 'empty-visible', 'empty-hidden', 'empty-system', 'empty-hidden-system')) {
            Invoke-Case "fresh stage original directory $OriginalShape remains recoverable or rejects before mutation" {
                $Game = Join-Path $TestRoot ('fresh-original-' + $OriginalShape)
                $OpenVrInputDestination = Join-Path $Game 'cojvr_openvr_input'
                $OpenVrInputBackup = Join-Path $Game 'cojvr_openvr_input.cojvr-backup'
                [void][IO.Directory]::CreateDirectory($OpenVrInputDestination)
                $OriginalFile = Join-Path $OpenVrInputDestination 'original.json'
                if ($OriginalShape -ne 'empty-root') { [IO.File]::WriteAllText($OriginalFile, 'original input data') }
                $Unsupported = $OriginalShape.StartsWith('empty-') -and $OriginalShape -ne 'empty-root'
                if ($Unsupported) {
                    $EmptyDirectory = Join-Path $OpenVrInputDestination 'user/empty'
                    [void][IO.Directory]::CreateDirectory($EmptyDirectory)
                    $Flags = [IO.FileAttributes]::Directory
                    if ($OriginalShape.Contains('hidden')) { $Flags = $Flags -bor [IO.FileAttributes]::Hidden }
                    if ($OriginalShape.Contains('system')) { $Flags = $Flags -bor [IO.FileAttributes]::System }
                    [IO.File]::SetAttributes($EmptyDirectory, $Flags)
                }
                # Execute the actual capture used by stage, then its journal
                # boundary, which precedes executable/proxy/input mutation.
                $IsNativeStereo = $true; $HadOriginalOpenVrInput = $true
                $JournalAssets = @()
                $Hash = [Security.Cryptography.SHA256]::Create()
                try { $StagedHash = [BitConverter]::ToString($Hash.ComputeHash([Text.Encoding]::UTF8.GetBytes('new actions'))).Replace('-', '') }
                finally { $Hash.Dispose() }
                $StagedOpenVrInputManifest = @{files=@(@{path='actions.json';sha256=$StagedHash})}
                Invoke-Expression $OriginalInputBlock.Extent.Text
                $Rejected = $false
                try { [void](Write-CojvrDeploymentJournal $Game 'stage' 'fresh-directory' 'd3d9_native_stereo' $JournalAssets) }
                catch { $Rejected = $true }
                $Journal = Join-Path $Game '.cojvr-deployment-transaction.json'
                if ($Unsupported) {
                    Assert-True $Rejected 'Fresh stage accepted an original empty subdirectory that its journal cannot restore.'
                    Assert-True (-not [IO.File]::Exists($Journal)) 'Rejected fresh stage published an unrecoverable journal.'
                    Assert-True (-not [IO.Directory]::Exists($OpenVrInputBackup)) 'Rejected fresh stage moved the original input.'
                    Assert-True ([IO.Directory]::Exists($EmptyDirectory)) 'Rejected fresh stage lost the original empty directory.'
                    Assert-True (([IO.File]::GetAttributes($EmptyDirectory) -band $Flags) -eq $Flags) 'Rejected fresh stage changed original directory flags.'
                    Assert-True ([IO.File]::ReadAllText($OriginalFile) -eq 'original input data') 'Rejected fresh stage changed original input bytes.'
                    return
                }
                Assert-True (-not $Rejected) 'Fresh stage rejected a representable original directory.'
                Move-Item -LiteralPath $OpenVrInputDestination -Destination $OpenVrInputBackup
                [void][IO.Directory]::CreateDirectory($OpenVrInputDestination)
                [IO.File]::WriteAllText((Join-Path $OpenVrInputDestination 'actions.json'), 'new actions')
                Assert-True (Complete-CojvrDeploymentRecovery $Game) 'Fresh directory transaction was not recoverable.'
                Assert-True (-not [IO.File]::Exists($Journal) -and -not [IO.Directory]::Exists($OpenVrInputBackup)) 'Fresh directory recovery retained completed transaction state.'
                Assert-True ([IO.Directory]::Exists($OpenVrInputDestination)) 'Recovery lost the original directory root.'
                if ($OriginalShape -eq 'files') {
                    Assert-True ([IO.File]::ReadAllText($OriginalFile) -eq 'original input data') 'Fresh recovery lost original input data.'
                } else {
                    Assert-True ([IO.Directory]::GetFileSystemEntries($OpenVrInputDestination).Count -eq 0) 'Fresh recovery failed to restore the empty original root.'
                }
            }
        }
        $StageText = $StageAst.Extent.Text
        Assert-True (([regex]::Matches($StageText, '& \$PythonExecutable -B \(Join-Path')).Count -eq 3 -and
            -not $StageText.Contains('& python ')) 'Patchers must use the selected Python with -B.'
        foreach ($Mode in @('valid', 'missing', 'hash', 'unknown-artifact', 'duplicate-artifact', 'duplicate-binding', 'unsafe-relative', 'absolute', 'case-alias', 'unknown-schema', 'controller-mismatch')) {
            Invoke-Case "input preflight and publish $Mode" {
                $ArtifactRoot = Join-Path $TestRoot ('input-source-' + $Mode)
                $OpenVrInputSource = Join-Path $ArtifactRoot 'cojvr_openvr_input'
                New-Item -ItemType Directory -Path $ArtifactRoot | Out-Null
                Copy-Item -LiteralPath (Join-Path $SourceDirectory 'assets/openvr') -Destination $OpenVrInputSource -Recurse
                $ProxyPath = Join-Path $ArtifactRoot 'd3d9_native_stereo.dll'
                # Dot-source import must preserve the caller's selected artifact.
                . (Join-Path $SourceDirectory 'tools/openvr_input_manifest.ps1')
                Assert-True ($ProxyPath -eq (Join-Path $ArtifactRoot 'd3d9_native_stereo.dll')) 'Input helper import replaced selected proxy path.'
                $Inventory = Get-CojvrOpenVrInputManifest $OpenVrInputSource
                $BuildManifest = [pscustomobject]@{openVrInputManifest=$Inventory; artifacts=@(
                    foreach ($File in $Inventory.files) {
                        $Role = if ($File.path -eq 'actions.json') {'openvr_action_manifest'} elseif ($File.path -eq 'bindings/psvr2_sense.json') {'openvr_binding_psvr2_sense'} else {'openvr_binding'}
                        [pscustomobject]@{role=$Role; fileName=('cojvr_openvr_input/' + $File.path); sha256=$File.sha256}
                    }
                )}
                $Game = Join-Path $TestRoot ('input-destination-' + $Mode)
                New-Item -ItemType Directory -Path $Game | Out-Null
                $OpenVrInputDestination = Join-Path $Game 'cojvr_openvr_input'
                $OpenVrInputBackup = Join-Path $Game 'cojvr_openvr_input.cojvr-backup'
                $OpenVrInputTemporary = Join-Path $Game 'cojvr_openvr_input.cojvr-installing'
                $IsNativeStereo = $true; $HadOriginalOpenVrInput = $false
                if ($Mode -eq 'missing') { Remove-Item -LiteralPath (Join-Path $OpenVrInputSource 'bindings/knuckles.json') }
                if ($Mode -eq 'hash') { [IO.File]::AppendAllText((Join-Path $OpenVrInputSource 'bindings/knuckles.json'), ' ') }
                if ($Mode -eq 'unknown-artifact') { $BuildManifest.artifacts[-1].fileName = 'cojvr_openvr_input/bindings/unknown.json' }
                if ($Mode -eq 'duplicate-artifact') { $BuildManifest.artifacts += $BuildManifest.artifacts[-1] }
                if ($Mode -eq 'unknown-schema') { $BuildManifest.openVrInputManifest.schemaVersion = 2 }
                if ($Mode -eq 'controller-mismatch') { Write-Json (Join-Path $OpenVrInputSource 'bindings/knuckles.json') @{controller_type='other'} }
                if ($Mode -in @('duplicate-binding', 'unsafe-relative', 'absolute', 'case-alias')) {
                    $ActionPath = Join-Path $OpenVrInputSource 'actions.json'
                    $Actions = Get-Content $ActionPath -Raw -Encoding UTF8 | ConvertFrom-Json
                    if ($Mode -eq 'duplicate-binding') { $Actions.default_bindings += $Actions.default_bindings[-1] }
                    if ($Mode -eq 'unsafe-relative') { $Actions.default_bindings[-1].binding_url = '../outside.json' }
                    if ($Mode -eq 'absolute') { $Actions.default_bindings[-1].binding_url = 'C:/outside.json' }
                    if ($Mode -eq 'case-alias') { $Actions.default_bindings[-1].binding_url = 'bindings/PSVR2_SENSE.json' }
                    Write-Json $ActionPath $Actions
                }
                if ($Mode -ne 'valid') {
                    $Before = @(Get-CojvrDirectoryManifest $Game)
                    $Rejected = $false
                    try { Invoke-Expression $Preflight.Extent.Text } catch { $Rejected = $true }
                    Assert-True $Rejected "Input preflight accepted $Mode."
                    Assert-True (Test-CojvrDirectoryMatchesManifest $Game $Before) 'Input rejection mutated destination.'
                    return
                }
                Invoke-Expression $Preflight.Extent.Text
                $JournalAssets = @(@{role='openvr_input'; stagedManifest=$StagedOpenVrInputManifest.files})
                Invoke-Expression $Publish.Extent.Text
                Assert-True (Test-CojvrDirectoryMatchesManifest $OpenVrInputDestination @($Inventory.files)) 'Stage failed to deploy all declared bindings.'
                $Deployment = @(); $IndependentHands = $false
                Invoke-Expression $DeploymentBlock.Extent.Text
                $InputDeployment = @($Deployment | Where-Object { $_.destination -like 'cojvr_openvr_input/*' })
                Assert-True ($InputDeployment.Count -eq $Inventory.files.Count) 'Run provenance omitted input artifacts.'
                foreach ($File in $Inventory.files) {
                    $Matches = @($InputDeployment | Where-Object { $_.destination -ceq ('cojvr_openvr_input/' + $File.path) -and $_.sha256 -ceq $File.sha256 })
                    Assert-True ($Matches.Count -eq 1) 'Run provenance has wrong/duplicate input identity.'
                }
            }
        }
        foreach ($Mode in @('valid', 'tampered-binding', 'tampered-backup', 'extra-file', 'unsafe-path', 'duplicate-path', 'bad-schema')) {
            Invoke-Case "generic input inventory $Mode" {
                $Game = New-StagedFixture ('generic-' + $Mode)
                $Input = Join-Path $Game 'cojvr_openvr_input'
                [IO.File]::WriteAllText((Join-Path $Input 'bindings/extra_controller.json'), 'experimental-binding')
                $State = Join-Path $Game '.cojvr-d3d9-stage.json'
                $Stage = Get-Content $State -Raw | ConvertFrom-Json
                $Inventory = [pscustomobject]@{schemaVersion=1; files=@(Get-CojvrDirectoryManifest $Input)}
                $Stage | Add-Member stagedOpenVrInputManifest $Inventory
                if ($Mode -eq 'unsafe-path') { $Inventory.files[0].path = '../outside.json' }
                if ($Mode -eq 'duplicate-path') { $Inventory.files += $Inventory.files[0] }
                if ($Mode -eq 'bad-schema') { $Inventory.schemaVersion = 999 }
                Write-Json $State $Stage
                if ($Mode -eq 'tampered-binding') { [IO.File]::AppendAllText((Join-Path $Input 'bindings/extra_controller.json'), 'tamper') }
                if ($Mode -eq 'tampered-backup') { [IO.File]::AppendAllText((Join-Path $Game 'cojvr_openvr_input.cojvr-backup/custom.json'), 'tamper') }
                if ($Mode -eq 'extra-file') { [IO.File]::WriteAllText((Join-Path $Input 'unowned.txt'), 'foreign') }
                if ($Mode -ne 'valid') { Assert-RejectedWithoutMutation $Game; return }
                & (Join-Path $SourceDirectory 'tools/unstage_d3d9_proxy.ps1') -GameDirectory $Game | Out-Null
                Assert-True (Test-CojvrDirectoryMatchesManifest $Input @($Stage.originalOpenVrInputManifest)) 'Generic inventory failed full restoration.'
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
