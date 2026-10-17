param(
    [string]$SourceDirectory = (Split-Path $PSScriptRoot -Parent),
    [string]$BinaryDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'work/resolution-selector-20261017')
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
$Helper = Join-Path $SourceDirectory 'tools/coj_resolution_settings.ps1'
if (-not (Test-Path -LiteralPath $Helper -PathType Leaf)) {
    throw 'Resolution settings helper is not implemented.'
}
. $Helper
$Root = Join-Path $BinaryDirectory ('settings-' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($Root)
$Path = Join-Path $Root 'launcher/resolution.json'
$Utf8 = New-Object Text.UTF8Encoding($false)
$script:Checks = 0
$Succeeded = $false
function Assert-True($Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
    $script:Checks++
}
function Assert-Rejected([scriptblock]$Operation, [string]$Message) {
    $Rejected = $false
    try { & $Operation | Out-Null } catch { $Rejected = $true }
    Assert-True $Rejected $Message
}
function Assert-Settings($Settings, [string]$Profile, [int]$Width = 0, [int]$Height = 0) {
    Assert-True ($Settings.schemaVersion -ceq 1 -and $Settings.profile -ceq $Profile -and
        $Settings.width -ceq $Width -and $Settings.height -ceq $Height) 'Settings values differ.'
    Assert-True ($Settings.width -is [int] -and $Settings.height -is [int] -and
        @($Settings.PSObject.Properties).Count -eq 4) 'Settings shape or integer types differ.'
}
function Set-Fixture([string]$Json) { [IO.File]::WriteAllText($Path, $Json, $Utf8) }
function Assert-Previous([string]$Expected) {
    Assert-True ([Convert]::ToBase64String([IO.File]::ReadAllBytes($Path)) -ceq $Expected) 'Previous bytes changed on failure.'
    Assert-True (@([IO.Directory]::GetFiles([IO.Path]::GetDirectoryName($Path))).Count -eq 1) 'Temporary file leaked.'
}

$OriginalLocal = $env:LOCALAPPDATA
try {
    $env:LOCALAPPDATA = Join-Path $Root 'local-app-data'
    $Expected = Join-Path $env:LOCALAPPDATA 'CoJVR/CallOfJuarez/resolution.json'
    Assert-True ((Get-CoJResolutionSettingsPath) -ceq $Expected) 'Default launcher path differs.'
    Assert-True (-not (Test-Path -LiteralPath $env:LOCALAPPDATA)) 'Getting path created files.'
    Assert-Settings (Read-CoJResolutionSettings -Path $Path) Preserve
    Assert-True (-not (Test-Path -LiteralPath $Path)) 'Missing read created settings.'

    # A real adjacent runtime preference must survive every save and fault.
    $Cinema = Join-Path $env:LOCALAPPDATA 'CoJVR/CallOfJuarez/settings.ini'
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Cinema))
    [IO.File]::WriteAllBytes($Cinema, [byte[]](0xEF,0xBB,0xBF,0x5B,0x76,0x72,0x5D,13,10,0xFF))
    $CinemaBefore = [Convert]::ToBase64String([IO.File]::ReadAllBytes($Cinema))
    foreach ($Profile in @('Preserve','Auto','75%','100%','125%')) {
        Assert-Settings (Write-CoJResolutionSettings -Path $Path -Profile $Profile) $Profile
        Assert-Settings (Read-CoJResolutionSettings -Path $Path) $Profile
        Assert-Settings (Write-CoJResolutionSettings -Path (Get-CoJResolutionSettingsPath) -Profile $Profile) $Profile
    }
    Assert-Settings (Write-CoJResolutionSettings -Path $Path -Profile Manual -Width '2804' -Height 2860) Manual 2804 2860
    Assert-Settings (Read-CoJResolutionSettings -Path $Path) Manual 2804 2860
    Assert-Settings (Write-CoJResolutionSettings -Path $Path -Profile Manual -Width 2147483647 -Height 1) Manual 2147483647 1
    $Previous = [Convert]::ToBase64String([IO.File]::ReadAllBytes($Path))
    foreach ($Value in @($null,$true,$false,1.0,[single]1,[decimal]1,-1,'-1','+1',' 1','1 ',
        '1.0','1e2','1x','',2147483648,[uint64]::MaxValue,@(1,2),[pscustomobject]@{value=1})) {
        Assert-Rejected { Write-CoJResolutionSettings -Path $Path -Profile Manual -Width $Value -Height 1 } 'Invalid width admitted.'
        Assert-Rejected { Write-CoJResolutionSettings -Path $Path -Profile Manual -Width 1 -Height $Value } 'Invalid height admitted.'
        Assert-Previous $Previous
    }
    foreach ($Operation in @(
        { Write-CoJResolutionSettings -Path $Path -Profile bogus },
        { Write-CoJResolutionSettings -Path $Path -Profile Manual },
        { Write-CoJResolutionSettings -Path $Path -Profile Manual -Width 1 },
        { Write-CoJResolutionSettings -Path $Path -Profile Manual -Width 0 -Height 1 },
        { Write-CoJResolutionSettings -Path $Path -Profile Auto -Width 1 },
        { Write-CoJResolutionSettings -Path $Path -Profile Preserve -Height 1 },
        { Write-CoJResolutionSettings -Path $Path -Profile '75%' -Width 1 -Height 1 },
        { Write-CoJResolutionSettings -Path $Path -Profile '100%' -Width $false },
        { Write-CoJResolutionSettings -Path $Path -Profile '125%' -Height 0.0 }
    )) {
        Assert-Rejected $Operation 'Invalid profile or stray dimensions admitted.'
        Assert-Previous $Previous
    }
    foreach ($Json in @(
        '', '{}', 'null', '[]', '{', '{"schemaVersion":1,"profile":"Auto","width":0,"height":0,}',
        '{"schemaVersion":2,"profile":"Auto","width":0,"height":0}',
        '{"schemaVersion":true,"profile":"Auto","width":0,"height":0}',
        '{"schemaVersion":1.0,"profile":"Auto","width":0,"height":0}',
        '{"schemaVersion":1,"profile":"Unknown","width":0,"height":0}',
        '{"schemaVersion":1,"profile":"Manual","width":0,"height":1}',
        '{"schemaVersion":1,"profile":"Auto","width":1,"height":0}',
        '{"schemaVersion":1,"profile":"Auto","width":false,"height":0}',
        '{"schemaVersion":1,"profile":"Manual","width":1.0,"height":1}',
        '{"schemaVersion":1,"profile":"Manual","width":1e2,"height":1}',
        '{"schemaVersion":1,"profile":"Manual","width":01,"height":1}',
        '{"schemaVersion":1,"profile":"Manual","width":-1,"height":1}',
        '{"schemaVersion":1,"profile":"Manual","width":"1","height":1}',
        '{"schemaVersion":1,"profile":"Manual","width":2147483648,"height":1}',
        '{"schemaVersion":1,"profile":"Auto","width":0,"height":0,"foreign":1}',
        '{"schemaVersion":1,"profile":"Auto","width":0,"height":0,"width":0}',
        '{"schemaVersion":1,"profile":"Auto","profile":"Auto","width":0,"height":0}',
        '{"schemaVersion":1,"profile":"Auto","width":0,"height":0,"Width":0}',
        '{"schemaVersion":1,"profile":"Auto","width":0,"height":0,"\u0077idth":0}',
        '{"schemaVersion":1,"profile":"Auto","width":{},"height":0}',
        '{"schemaVersion":1,"profile":"Auto","width":0}',
        '{"schemaVersion":1,"profile":"Auto","width":0,"height":0} garbage',
        "{`n// comment`n}", (' ' * 4097)
    )) {
        Set-Fixture $Json
        Assert-Rejected { Read-CoJResolutionSettings -Path $Path } "Invalid JSON admitted: $Json"
    }
    foreach ($Key in @('schemaVersion','profile','width','height')) {
        Set-Fixture ('{"schemaVersion":1,"profile":"Auto","width":0,"height":0,"' + $Key + '":0}')
        Assert-Rejected { Read-CoJResolutionSettings -Path $Path } "Duplicate $Key admitted."
    }
    $Canonical = '{"schemaVersion":1,"profile":"Preserve","width":0,"height":0}'
    Set-Fixture ($Canonical + (' ' * (4096 - $Canonical.Length)))
    Assert-Settings (Read-CoJResolutionSettings -Path $Path) Preserve
    [IO.File]::AppendAllText($Path, ' ', $Utf8)
    Assert-Rejected { Read-CoJResolutionSettings -Path $Path } 'Oversized valid JSON admitted.'
    Set-Fixture "{`r`n`t`"height`":2860, `"width`":2804, `"profile`":`"Manual`", `"schemaVersion`":1}"
    Assert-Settings (Read-CoJResolutionSettings -Path $Path) Manual 2804 2860
    [IO.File]::WriteAllBytes($Path, [byte[]](0xFF,0xFE,0x00))
    Assert-Rejected { Read-CoJResolutionSettings -Path $Path } 'Invalid UTF-8 admitted.'

    Write-CoJResolutionSettings -Path $Path -Profile Auto | Out-Null
    $Previous = [Convert]::ToBase64String([IO.File]::ReadAllBytes($Path))
    $Lock = [IO.File]::Open($Path, 'Open', 'Read', 'Read')
    try {
        Assert-Rejected { Write-CoJResolutionSettings -Path $Path -Profile Manual -Width 1 -Height 1 } 'Locked replacement succeeded.'
        Assert-Previous $Previous
    } finally { $Lock.Dispose() }
    [IO.File]::SetAttributes($Path, [IO.FileAttributes]::ReadOnly)
    try {
        Assert-Rejected { Write-CoJResolutionSettings -Path $Path -Profile Preserve } 'Read-only replacement succeeded.'
        Assert-Previous $Previous
    } finally { [IO.File]::SetAttributes($Path, [IO.FileAttributes]::Normal) }
    $Blocked = Join-Path $Path 'child.json'
    Assert-Rejected { Write-CoJResolutionSettings -Path $Blocked -Profile Auto } 'File used as parent directory.'
    Assert-Previous $Previous
    Assert-Rejected { Read-CoJResolutionSettings -Path ([IO.Path]::GetDirectoryName($Path)) } 'Directory read admitted.'
    Assert-Rejected { Write-CoJResolutionSettings -Path ($Path + ':stream') -Profile Auto } 'Alternate data stream admitted.'

    # Junctions need no elevated symlink privilege on Windows.
    $Junction = Join-Path $Root 'junction'
    New-Item -ItemType Junction -Path $Junction -Target ([IO.Path]::GetDirectoryName($Path)) | Out-Null
    try {
        $Linked = Join-Path $Junction 'resolution.json'
        Assert-Rejected { Read-CoJResolutionSettings -Path $Linked } 'Reparse ancestor read admitted.'
        Assert-Rejected { Read-CoJResolutionSettings -Path (Join-Path $Junction 'missing.json') } 'Missing leaf bypassed reparse check.'
        Assert-Rejected { Write-CoJResolutionSettings -Path $Linked -Profile Preserve } 'Reparse ancestor write admitted.'
        Assert-Rejected { Write-CoJResolutionSettings -Path (Join-Path $Junction 'new/leaf.json') -Profile Auto } 'Missing parents bypassed reparse check.'
        Assert-Rejected { Read-CoJResolutionSettings -Path $Junction } 'Reparse leaf read admitted.'
        Assert-Rejected { Write-CoJResolutionSettings -Path $Junction -Profile Auto } 'Reparse leaf write admitted.'
        Assert-Previous $Previous
    } finally { [IO.Directory]::Delete($Junction) }
    Assert-True ([Convert]::ToBase64String([IO.File]::ReadAllBytes($Cinema)) -ceq $CinemaBefore) 'Runtime cinema settings changed.'
    $Succeeded = $true
    Write-Host "PASS: $script:Checks resolution-settings checks; PowerShell $($PSVersionTable.PSVersion)."
} finally {
    $env:LOCALAPPDATA = $OriginalLocal
    # Retain failed fixtures for diagnosis; successful checks leave no stale tree.
    if ($Succeeded) {
        $Allowed = [IO.Path]::GetFullPath($BinaryDirectory).TrimEnd('\') + '\'
        if (-not ([IO.Path]::GetFullPath($Root)).StartsWith($Allowed,[StringComparison]::OrdinalIgnoreCase)) {
            throw 'Resolution settings fixture cleanup escaped BinaryDirectory.'
        }
        Remove-Item -LiteralPath $Root -Recurse -Force
    }
}
