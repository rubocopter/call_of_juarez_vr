param([string]$SourceDirectory = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $SourceDirectory 'release/release_common.ps1')

function Assert-True([bool]$Condition,[string]$Message) { if (-not $Condition) { throw $Message } }

$Fixture = Join-Path $SourceDirectory ('work/release-discovery-' + [guid]::NewGuid().ToString('N'))
try {
    $Root = Join-Path $Fixture 'Steam main'
    $Extra = Join-Path $Fixture 'Steam games & extra'
    $AppRoot = Join-Path $Extra 'steamapps'
    $Game = Join-Path $AppRoot 'common/CoJ Alternate Folder'
    [void](New-Item -ItemType Directory -Path $Root,$AppRoot,$Game,(Join-Path $Root 'steamapps') -Force)
    [IO.File]::WriteAllText((Join-Path $Game 'CoJ.exe'),'fake game')
    [IO.File]::WriteAllText((Join-Path $Game 'ChromeEngine3.dll'),'fake engine')
    $EscapedExtra = $Extra.Replace('\','\\')
    $Vdf = '"libraryfolders"' + "`n{" + "`n" + '"1" { "path" "' + $EscapedExtra + '" "apps" { "3020" "1" } }' + "`n}"
    [IO.File]::WriteAllText((Join-Path $Root 'steamapps/libraryfolders.vdf'),$Vdf)
    # Steam allows a different installdir from the display title.
    [IO.File]::WriteAllText((Join-Path $AppRoot 'appmanifest_3020.acf'), '"AppState" { "appid" "3020" "installdir" "CoJ Alternate Folder" }')
    $Found = @(Get-CojvrPublicGameCandidates -SteamRoots @($Root))
    Assert-True ($Found.Count -eq 1 -and $Found[0] -eq $Game) 'Manifest-based custom install dir in additional Steam library was not detected.'

    $Fallback = Join-Path $Root 'steamapps/common/Call of Juarez'
    [void](New-Item -ItemType Directory -Path $Fallback -Force)
    [IO.File]::WriteAllText((Join-Path $Fallback 'CoJ.exe'),'fallback game')
    [IO.File]::WriteAllText((Join-Path $Fallback 'ChromeEngine3.dll'),'fallback engine')
    $Found = @(Get-CojvrPublicGameCandidates -SteamRoots @($Root,$Root))
    Assert-True ($Found.Count -eq 2) 'Multiple installations were lost or duplicate Steam roots added duplicates.'
    $Failed = $false
    try { Resolve-CojvrPublicGame '' -SteamRoots @($Root) | Out-Null } catch { $Failed = $true }
    Assert-True $Failed 'Ambiguous Steam installations were silently selected.'
    Remove-Item -LiteralPath (Join-Path $Game 'ChromeEngine3.dll')
    $Found = @(Get-CojvrPublicGameCandidates -SteamRoots @($Root))
    Assert-True ($Found.Count -eq 1 -and $Found[0] -eq $Fallback) 'Incomplete install was falsely detected.'
    [IO.File]::WriteAllText((Join-Path $AppRoot 'appmanifest_3020.acf'), '"AppState" { "appid" "3020" "installdir" "..\\outside" }')
    $Found = @(Get-CojvrPublicGameCandidates -SteamRoots @($Root))
    Assert-True ($Found.Count -eq 1 -and $Found[0] -eq $Fallback) 'Unsafe install dir escaped steamapps/common.'
    $Gog = Join-Path $Fixture 'GOG copy'
    [void](New-Item -ItemType Directory -Path $Gog -Force)
    [IO.File]::WriteAllText((Join-Path $Gog 'CoJ.exe'),'gog game')
    [IO.File]::WriteAllText((Join-Path $Gog 'ChromeEngine3.dll'),'gog engine')
    $Found = @(Get-CojvrPublicGameCandidates -SteamRoots @($Root) -GogRoots @($Gog,$Gog))
    Assert-True ($Found.Count -eq 2 -and $Gog -in $Found -and $Fallback -in $Found) 'Steam/GOG copies were not independently selectable or deduplicated.'
    $Failed = $false
    try { Resolve-CojvrPublicGame '' -SteamRoots @($Root) -GogRoots @($Gog) | Out-Null } catch { $Failed = $true }
    Assert-True $Failed 'Ambiguous Steam/GOG installations were silently selected.'
    Assert-True ((Resolve-CojvrPublicGame $Gog) -eq $Gog) 'Manual GOG selection failed.'
    Remove-Item -LiteralPath (Join-Path $Gog 'ChromeEngine3.dll')
    $Found = @(Get-CojvrPublicGameCandidates -SteamRoots @($Root) -GogRoots @($Gog))
    Assert-True ($Found.Count -eq 1 -and $Found[0] -eq $Fallback) 'Incomplete GOG install was admitted.'
    Write-Host 'PASS release discovery: Steam libraries/manifest, GOG, mixed installs, ambiguity, manual paths, deduplication, incomplete and unsafe paths.'
} finally {
    if (Test-Path -LiteralPath $Fixture) { Remove-Item -LiteralPath $Fixture -Recurse -Force }
}
