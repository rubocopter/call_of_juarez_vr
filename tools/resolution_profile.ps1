# Local development selector; public packages expose the same preference via option 7.
[CmdletBinding()]
param(
    [ValidateSet('Menu','Set','Prepare','Status')][string]$Action='Menu',
    [ValidateSet('Preserve','Auto','75%','100%','125%','Manual')][string]$Profile='Preserve',
    [object]$Width=0,[object]$Height=0,
    [string]$SettingsPath='', [string]$GameDirectory=''
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'coj_resolution_settings.ps1')
if ([string]::IsNullOrWhiteSpace($SettingsPath)) {$SettingsPath=Get-CoJResolutionSettingsPath}
function Prepare-SelectedResolution {
    $Selected=Read-CoJResolutionSettings -Path $SettingsPath
    $Arguments=@('-NoProfile','-File',(Join-Path $PSScriptRoot 'vr_test.ps1'),'prepare',
        '-BodyIkAtStart','-LargeAddressAware','-ManualReload','-UseSavedResolutionProfile','-ResolutionSettingsPath',$SettingsPath)
    if ($GameDirectory) {$Arguments+=@('-GameDirectory',$GameDirectory)}
    if ($Selected.profile -eq 'Manual' -and $Selected.width -eq 2804 -and $Selected.height -eq 2860) {
        $Arguments+='-MenuPresentationCheck'
    }
    & pwsh @Arguments
    if ($LASTEXITCODE -ne 0) {throw 'Preparation failed. See the message above; any existing candidate must be finished first.'}
}
switch ($Action) {
    Set {[void](Write-CoJResolutionSettings -Path $SettingsPath -Profile $Profile -Width $Width -Height $Height)}
    Status {Read-CoJResolutionSettings -Path $SettingsPath | Format-List}
    Prepare {Prepare-SelectedResolution}
    Menu {
        while ($true) {
            $Selected=Read-CoJResolutionSettings -Path $SettingsPath
            $Extent=if($Selected.profile -eq 'Manual'){" $($Selected.width)x$($Selected.height)"}else{''}
            Write-Host "`nPerfil guardado: $($Selected.profile)$Extent"
            Write-Host '1 Conservar | 2 Auto | 3 75% | 4 100% | 5 125% | 6 Manual | 7 Preparar prueba | 0 Salir'
            Write-Host 'Perfiles experimentales al preparar con el juego cerrado. Auto/porcentajes consultan SteamVR abierto; los porcentajes escalan cantidad de pixeles.'
            $Choice=Read-Host 'Opcion'
            if ($Choice -eq '0') {break}
            try {
                if ($Choice -eq '7') {Prepare-SelectedResolution;continue}
                $Chosen=@{'1'='Preserve';'2'='Auto';'3'='75%';'4'='100%';'5'='125%';'6'='Manual'}[$Choice]
                if (-not $Chosen) {continue}
                $W=0;$H=0
                if ($Chosen -eq 'Manual') {$W=Read-Host 'Anchura por ojo';$H=Read-Host 'Altura por ojo'}
                [void](Write-CoJResolutionSettings -Path $SettingsPath -Profile $Chosen -Width $W -Height $H)
                Write-Host 'Guardado. Opcion 7 para preparar; si ya hay candidato, cierra el juego y ejecuta finish antes.'
            } catch {Write-Host ('ERROR: '+$_.Exception.Message) -ForegroundColor Red}
        }
    }
}
