param(
    [Parameter(Mandatory = $true)]
    [string]$GameDirectory,

    [string]$ProxyPath = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ProxyPath)) {
    $ProxyPath = Join-Path $PSScriptRoot "..\build\win32-debug\Release\d3d9.dll"
}

$ExpectedCoJHash = "5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE"
$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$ProxyPath = [System.IO.Path]::GetFullPath($ProxyPath)
$Executable = Join-Path $GameDirectory "CoJ.exe"
$Destination = Join-Path $GameDirectory "d3d9.dll"
$Backup = Join-Path $GameDirectory "d3d9.cojvr-backup.dll"
$Log = Join-Path $GameDirectory "cojvr.log"
$State = Join-Path $GameDirectory ".cojvr-d3d9-stage.json"

if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "CoJ.exe was not found in '$GameDirectory'."
}

if (-not (Test-Path -LiteralPath $ProxyPath -PathType Leaf)) {
    throw "Release proxy was not found at '$ProxyPath'. Build the Release preset first."
}

$ActualHash = (Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash.ToUpperInvariant()
if ($ActualHash -ne $ExpectedCoJHash) {
    throw "CoJ.exe SHA-256 is not the supported build. Expected $ExpectedCoJHash, got $ActualHash."
}

if (Test-Path -LiteralPath $Backup) {
    throw "Backup '$Backup' already exists. Restore or remove it before staging another proxy."
}

if (Test-Path -LiteralPath $State) {
    throw "A previous CoJ VR staging state already exists at '$State'. Unstage it first."
}

$HadOriginal = Test-Path -LiteralPath $Destination
if ($HadOriginal) {
    Move-Item -LiteralPath $Destination -Destination $Backup
    Write-Host "Backed up existing d3d9.dll to d3d9.cojvr-backup.dll"
}

try {
    Copy-Item -LiteralPath $ProxyPath -Destination $Destination
    @{
        hadOriginalD3D9 = $HadOriginal
        stagedProxySha256 = (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash.ToUpperInvariant()
    } | ConvertTo-Json | Set-Content -LiteralPath $State -Encoding UTF8
} catch {
    if (Test-Path -LiteralPath $Destination) {
        Remove-Item -LiteralPath $Destination -Force
    }
    if (Test-Path -LiteralPath $Backup) {
        Move-Item -LiteralPath $Backup -Destination $Destination
    }
    throw
}

if (Test-Path -LiteralPath $Log) {
    Remove-Item -LiteralPath $Log -Force
}

$ProxyLeaf = [System.IO.Path]::GetFileName($ProxyPath)
$IsReadbackDiagnostic = $ProxyLeaf -ieq "d3d9_readback.dll"
$IsOpenVrFlatDiagnostic = $ProxyLeaf -ieq "d3d9_openvr_flat.dll"

if ($IsReadbackDiagnostic) {
    Write-Host "Staged CoJ VR classic-D3D9 readback diagnostic proxy."
} elseif ($IsOpenVrFlatDiagnostic) {
    Write-Host "Staged CoJ VR EndScene-driven OpenVR flat diagnostic proxy."
} else {
    Write-Host "Staged CoJ VR D3D9 forwarding proxy with Present/Reset observation hooks."
}
Write-Host "Build identity: $ActualHash"
Write-Host "Next: launch Call of Juarez manually, load a save, confirm normal rendering, then exit normally."
if ($IsReadbackDiagnostic) {
    Write-Host "After exit run tools\verify_d3d9_readback_live_test.ps1 -GameDirectory '$GameDirectory'."
} elseif ($IsOpenVrFlatDiagnostic) {
    Write-Host "After exit run tools\verify_d3d9_openvr_flat_live_test.ps1 -GameDirectory '$GameDirectory'."
} else {
    Write-Host "After exit run tools\verify_d3d9_live_test.ps1 -GameDirectory '$GameDirectory'."
}
