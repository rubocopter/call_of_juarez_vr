[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$HelperPath,
    [string]$RuntimeDirectory,
    [string]$ProbeDirectory=(Join-Path $PSScriptRoot '../build/win32-debug/Release')
)
$ErrorActionPreference='Stop'
$helper=(Resolve-Path -LiteralPath $HelperPath).Path
$session=(Resolve-Path -LiteralPath (Join-Path $ProbeDirectory 'cojvr_neural_session_test.exe')).Path
$gpu=(Resolve-Path -LiteralPath (Join-Path $ProbeDirectory 'cojvr_neural_gpu_roundtrip_test.exe')).Path
& $session $helper
if($LASTEXITCODE -ne 0){throw 'Neural IPC/identity/recovery tests failed'}
if($RuntimeDirectory){
    $runtime=(Resolve-Path -LiteralPath $RuntimeDirectory).Path
    & $gpu $helper $runtime
}else{
    & $gpu $helper
}
if($LASTEXITCODE -ne 0){throw 'Neural D3D11 roundtrip failed'}
if(-not $RuntimeDirectory){Write-Host 'Synthetic diagnostics passed. Real model inference was not requested.'}
else{Write-Host 'Synthetic diagnostics and local-model laboratory completed. Game/headset acceptance remains pending.'}
