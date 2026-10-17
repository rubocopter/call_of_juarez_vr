param([string]$SourceDirectory,[string]$BinaryDirectory)
$ErrorActionPreference='Stop'
$Root=Join-Path $BinaryDirectory ('resource-control-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $Root | Out-Null
@{diagnosticMode='d3d9_native_stereo'} | ConvertTo-Json | Set-Content (Join-Path $Root '.cojvr-d3d9-stage.json')
foreach($Probe in @($false,$true)) {
 foreach($Extent in @('off','2560x1440','2560x1920')) {
    @{enabled=$true;renderResourceProbeEnabled=$Probe;frameLumaProbeEnabled=$Probe;manualReloadInstalled=$true;independentHandsEnabled=$true;independentEyeResolution=$Extent} |
        ConvertTo-Json | Set-Content (Join-Path $Root 'cojvr-camera-control.json')
    foreach($Mode in @('enable','body-enable','recenter','body-disable','disable')) {
        & (Join-Path $SourceDirectory 'tools/set_hmd_camera_control.ps1') -GameDirectory $Root -Mode $Mode | Out-Null
        $Control=Get-Content (Join-Path $Root 'cojvr-camera-control.json') -Raw | ConvertFrom-Json
        if($Control.frameLumaProbeEnabled -ne $Probe -or $Control.renderResourceProbeEnabled -ne $Probe -or -not $Control.manualReloadInstalled -or
            -not $Control.independentHandsEnabled -or $Control.independentEyeResolution -ne $Extent) {throw "Control mode $Mode changed diagnostic/reload/hand/eye opt-in"}
    }
 }
}
Write-Host 'Startup diagnostic preference survives canonical control changes.'
