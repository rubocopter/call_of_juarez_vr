# Repository GUI bridge. Player ZIPs keep using their self-contained frontend.
# No game mutation happens until the ordinary packaged backend admits it.
#Requires -Version 5.1
param(
    [ValidateSet('Install','Uninstall','Diagnose','Status','Verify','PrepareSession')]
    [string]$Action,
    [string]$GameDirectory = '',
    [switch]$ManualReload = $true,
    [string]$VideoSettingsPath = '',
    [string]$ResolutionSettingsPath = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$Repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $Repository 'release/release_common.ps1')
try {
    [void](Get-CojvrResolutionSettingsHelper (Join-Path $Repository 'release'))
    $Build = Join-Path $Repository 'build/win32-public-release/Release'
    $BuildManifestPath = Join-Path $Build 'd3d9_native_stereo.build-manifest.json'
    if (-not (Test-Path -LiteralPath $BuildManifestPath -PathType Leaf)) {
        throw 'The local native build is missing. Build with tools/package_release.ps1 first, or use an extracted player ZIP.'
    }
    $BuildManifest = Get-Content -LiteralPath $BuildManifestPath -Raw | ConvertFrom-Json
    # Bind the cache to current installer/patcher inputs and native artifact
    # bytes. Old or altered packages cannot become a silent source fallback.
    $Inputs = @($BuildManifestPath)
    foreach ($Directory in @('release','tools')) {
        $Inputs += @(Get-ChildItem -LiteralPath (Join-Path $Repository $Directory) -Recurse -File |
            Where-Object { $_.Extension -in @('.ps1','.py','.cmd','.json','.txt','.md') } |
            ForEach-Object { $_.FullName })
    }
    $Inputs += @(Join-Path $Repository 'LICENSE'; Join-Path $Repository 'COPYRIGHT_AND_REUSE.md')
    $Inputs += @($BuildManifest.artifacts | ForEach-Object { Join-Path $Build $_.fileName })
    $Lines = @($Inputs | Sort-Object -Unique | ForEach-Object {
        Assert-CojvrNoReparse $_
        $_.Substring($Repository.Length) + ':' + (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash
    })
    $Hasher = [Security.Cryptography.SHA256]::Create()
    try { $Key = [BitConverter]::ToString($Hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes(($Lines -join "`n")))).Replace('-','').ToLowerInvariant() }
    finally { $Hasher.Dispose() }
    $Cache = Join-Path $Repository ('work/installer-packages/' + $Key)
    Assert-CojvrNoReparse $Cache
    New-Item -ItemType Directory -Path $Cache -Force | Out-Null
    $Lease = [IO.File]::Open((Join-Path $Cache 'package.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try {
        $Player = Join-Path $Cache 'player'
        Assert-CojvrNoReparse $Player
        if (-not (Test-Path -LiteralPath (Join-Path $Player 'release-manifest.json') -PathType Leaf)) {
            if (Test-Path -LiteralPath $Player) { throw "Incomplete local installer cache: $Player. Preserve it and rebuild the local package." }
            Write-Host 'Preparing the local installer package. This can take a few seconds...'
            # Retain existing native build provenance; do not relabel or rebuild
            # the tested payload merely to refresh its installer scripts.
            $Version = (Get-Content -LiteralPath (Join-Path $Repository 'release/README.txt') -TotalCount 1) -replace '^Call of Juarez VR ([0-9.]+-prealpha\.[0-9]+).*$','$1'
            $Output = Join-Path $Cache ('build-' + [guid]::NewGuid().ToString('N'))
            & (Join-Path $PSScriptRoot 'package_release.ps1') -Version $Version -SkipBuild -AllowDirty -OutputDirectory $Output
            $Zip = Join-Path $Output "CoJ-VR-$Version-windows-x64.zip"
            Expand-Archive -LiteralPath $Zip -DestinationPath $Player
        }
        [void](Read-CojvrRelease $Player)
        Write-Host 'Local installer package verified.'
    } finally { $Lease.Dispose() }
    $Forward = @{Action=$Action;GameDirectory=$GameDirectory;VideoSettingsPath=$VideoSettingsPath;ResolutionSettingsPath=$ResolutionSettingsPath}
    if ($PSBoundParameters.ContainsKey('ManualReload')) { $Forward.ManualReload = $ManualReload }
    # A successful PowerShell script need not set the automatic native exit
    # variable at all; do not inherit a stale packaging-program exit code.
    $global:LASTEXITCODE = 0
    # Match a standalone frontend process. The repository builder's strict-mode
    # setting must not change optional-property behavior in the public backend.
    Set-StrictMode -Off
    & (Join-Path $Player 'cojvr.ps1') @Forward
    exit $LASTEXITCODE
} catch {
    Write-Host ('ERROR: ' + $_.Exception.Message)
    exit 1
}
