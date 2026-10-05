$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../tools/coj_executable_identity.ps1')
$original = '5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE'
$derived = 'C8B8BB82FCB3D6599C5F77B1BB9CB3444CBB3A360461DAD43AD808B49AD28DC9'
$run = @{game=@{executable=@{sha256=$original;knownExactBuild=$true}};deployment=@()}
Assert-CoJExecutableProvenance $run @{}
$run.game.executable = @{sha256=$derived;originalSha256=$original;knownExactBuild=$true;largeAddressAware=$true}
$run.deployment = @(@{role='game_executable';destination='CoJ.exe';sha256=$derived;originalSha256=$original})
$stage = @{executableManaged=$true;stagedExecutableSha256=$derived;originalExecutableSha256=$original}
Assert-CoJExecutableProvenance $run $stage
function Must-Reject([scriptblock]$Action) {
    $rejected = $false
    try { & $Action } catch { $rejected = $true }
    if (-not $rejected) { throw 'Unproven executable provenance was accepted.' }
}
$stage.originalExecutableSha256 = 'unknown'
Must-Reject { Assert-CoJExecutableProvenance $run $stage }
$stage.originalExecutableSha256 = $original
$run.game.executable.largeAddressAware = $false
Must-Reject { Assert-CoJExecutableProvenance $run $stage }
$run.game.executable.largeAddressAware = $true
$run.deployment[0].sha256 = $original
Must-Reject { Assert-CoJExecutableProvenance $run $stage }
$run.deployment[0].sha256 = $derived
$run.deployment += $run.deployment[0]
Must-Reject { Assert-CoJExecutableProvenance $run $stage }
$run.deployment = @($run.deployment[0])
$run.game.executable.sha256 = $derived.Substring(0,63) + '0'
Must-Reject { Assert-CoJExecutableProvenance $run $stage }
$run.game.executable.sha256 = $original
$run.game.executable.largeAddressAware = $false
Must-Reject { Assert-CoJExecutableProvenance $run $stage }
Write-Host 'PASS: exact original/LAA identities, stage/backup/deployment correlation and near-hash rejection.'
