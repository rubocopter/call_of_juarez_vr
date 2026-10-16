# Builder-only bootstrap. The player frontend never downloads dependencies.
# Requires Windows PowerShell 5.1 or later on a Windows x64 host.
[CmdletBinding()]
param(
    [Alias('DependencyDirectory')][string]$Destination = '',
    [switch]$Offline
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $Destination) { $Destination = Join-Path $RepositoryRoot '.cache/release-deps' }
$Destination = [IO.Path]::GetFullPath($Destination)
if (-not [Environment]::Is64BitOperatingSystem) { throw 'Release dependencies require Windows x64.' }
$Lock = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'release/dependencies.lock.json') -Raw | ConvertFrom-Json
if ($Lock.schemaVersion -ne 1) { throw 'Unsupported dependency lock schema.' }
$DownloadDirectory = Join-Path $Destination 'downloads'
New-Item -ItemType Directory -Path $DownloadDirectory -Force | Out-Null
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12

function Assert-Hash([string]$Path, [string]$Expected) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf) -or
        (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -cne $Expected) {
        throw "Dependency SHA-256 mismatch or missing file: $Path"
    }
}

function Get-PinnedDownload($Dependency) {
    if ($Dependency.sha256 -cnotmatch '^[A-F0-9]{64}$' -or
        [IO.Path]::GetFileName($Dependency.fileName) -cne $Dependency.fileName) {
        throw 'Invalid dependency lock entry.'
    }
    $Path = Join-Path $DownloadDirectory $Dependency.fileName
    # A corrupt cache fails closed; it is never silently replaced with an update.
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        if ($Offline) { throw "Pinned dependency absent from offline cache: $Path" }
        $Partial = "$Path.$([Guid]::NewGuid().ToString('N')).partial"
        try {
            Invoke-WebRequest -UseBasicParsing -Uri $Dependency.url -OutFile $Partial
            Assert-Hash $Partial $Dependency.sha256
            Move-Item -LiteralPath $Partial -Destination $Path
        } finally {
            if (Test-Path -LiteralPath $Partial) { Remove-Item -LiteralPath $Partial -Force }
        }
    }
    Assert-Hash $Path $Dependency.sha256
    return $Path
}

$PythonArchive = Get-PinnedDownload $Lock.python
$VcInstaller = Get-PinnedDownload $Lock.vcRuntime
$Signature = Get-AuthenticodeSignature -LiteralPath $VcInstaller
if ($Signature.Status -ne 'Valid' -or $null -eq $Signature.SignerCertificate -or
    $Signature.SignerCertificate.Subject -cne $Lock.vcRuntime.authenticodeSubject) {
    throw 'Pinned VC++ redistributable does not have a valid Microsoft Authenticode signature.'
}
$ActualVersion = (Get-Item -LiteralPath $VcInstaller).VersionInfo.ProductVersion
if ($ActualVersion -ne $Lock.vcRuntime.version) { throw 'VC++ installer version differs from dependency lock.' }

# Always reconstruct from the verified archive, never trust a previously extracted tree.
$PythonDirectory = Join-Path $Destination 'python'
$Temporary = Join-Path $Destination ('.python-' + [Guid]::NewGuid().ToString('N'))
try {
    Expand-Archive -LiteralPath $PythonArchive -DestinationPath $Temporary
    foreach ($Name in @('python.exe', 'python313.dll', 'python313.zip', 'python313._pth', 'vcruntime140.dll', 'vcruntime140_1.dll', 'LICENSE.txt')) {
        if (-not (Test-Path -LiteralPath (Join-Path $Temporary $Name) -PathType Leaf)) {
            throw "Official embedded Python is incomplete: $Name"
        }
    }
    # Isolated Python does not add the script directory. All patcher-local imports
    # must work after relocation, without PYTHONPATH, site packages or system Python.
    [IO.File]::WriteAllText((Join-Path $Temporary 'python313._pth'), "python313.zip`n.`n../../tools`n", [Text.Encoding]::ASCII)
    $PythonVersion = & (Join-Path $Temporary 'python.exe') -B -c "import sys, struct; print(sys.version.split()[0]); assert struct.calcsize('P') == 8"
    if ($LASTEXITCODE -ne 0 -or $PythonVersion -ne $Lock.python.version) { throw 'Bundled Python startup/version check failed.' }
    if (Test-Path -LiteralPath $PythonDirectory) {
        $Resolved = (Get-Item -LiteralPath $PythonDirectory).FullName
        if ($Resolved -cne [IO.Path]::GetFullPath($PythonDirectory) -or
            (Get-Item -LiteralPath $PythonDirectory).Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw 'Refusing to replace a redirected Python dependency directory.'
        }
        Remove-Item -LiteralPath $Resolved -Recurse -Force
    }
    Move-Item -LiteralPath $Temporary -Destination $PythonDirectory
    Copy-Item -LiteralPath $VcInstaller -Destination (Join-Path $Destination 'vc_redist.x86.exe') -Force
    Assert-Hash (Join-Path $Destination 'vc_redist.x86.exe') $Lock.vcRuntime.sha256
} finally {
    if (Test-Path -LiteralPath $Temporary) {
        if (-not ([IO.Path]::GetFullPath($Temporary)).StartsWith($Destination + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe temporary path.' }
        Remove-Item -LiteralPath $Temporary -Recurse -Force
    }
}
Write-Host "Verified release dependencies: Python $($Lock.python.version) x64; VC++ $($Lock.vcRuntime.version) x86."
