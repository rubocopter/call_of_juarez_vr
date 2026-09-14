param([string]$Destination = "")

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($Destination)) {
    $Destination = Join-Path $repoRoot ".cache\deps\openvr-2.15.6"
}

$archiveUrl = "https://github.com/ValveSoftware/openvr/archive/refs/tags/v2.15.6.zip"
$archiveHash = "7629E6586338DBC0F877571870F04534E24EE752126606C5D8FFDD7C3E7D31D4"
$headerHash = "641EF9791FA8A281C490C85337CB3CBEA8929055CFAD1C78275D26CEF587884F"
$libraryHash = "9EC5921313D97EB88D7FB98371E31CC0EB73F792266F8EA4EF387031FE0D3A3D"
$dllHash = "AB696E4F218A95B3E396BC310F9FE6485DF48C99C0969762083212B1E1F025A6"
$downloadDir = Join-Path $repoRoot ".cache\downloads"
$archivePath = Join-Path $downloadDir "openvr-v2.15.6.zip"
$extractDir = Join-Path $repoRoot ".cache\deps\openvr-v2.15.6-extract"

function Assert-Hash([string]$Path, [string]$Expected) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing expected OpenVR file: $Path" }
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
    if ($actual -ne $Expected) { throw "SHA-256 mismatch for $Path. Expected $Expected, got $actual" }
}

$header = Join-Path $Destination "headers\openvr.h"
$library = Join-Path $Destination "lib\win32\openvr_api.lib"
$dll = Join-Path $Destination "bin\win32\openvr_api.dll"
if ((Test-Path $header) -and (Test-Path $library) -and (Test-Path $dll)) {
    try {
        Assert-Hash $header $headerHash
        Assert-Hash $library $libraryHash
        Assert-Hash $dll $dllHash
        Write-Host "OpenVR 2.15.6 dependency already verified at $Destination"
        exit 0
    } catch {}
}

New-Item -ItemType Directory -Force -Path $downloadDir | Out-Null
$downloadNeeded = -not (Test-Path -LiteralPath $archivePath -PathType Leaf)
if (-not $downloadNeeded) {
    $downloadNeeded = (Get-FileHash -Algorithm SHA256 -LiteralPath $archivePath).Hash -ne $archiveHash
}
if ($downloadNeeded) { Invoke-WebRequest -Uri $archiveUrl -OutFile $archivePath }
Assert-Hash $archivePath $archiveHash

New-Item -ItemType Directory -Force -Path $extractDir | Out-Null
Expand-Archive -LiteralPath $archivePath -DestinationPath $extractDir -Force
$sourceRoot = Join-Path $extractDir "openvr-2.15.6"
if (-not (Test-Path -LiteralPath $sourceRoot -PathType Container)) { throw "Unexpected OpenVR archive layout" }

New-Item -ItemType Directory -Force -Path $Destination | Out-Null
Copy-Item -LiteralPath (Join-Path $sourceRoot "headers") -Destination $Destination -Recurse -Force
New-Item -ItemType Directory -Force -Path (Join-Path $Destination "lib") | Out-Null
Copy-Item -LiteralPath (Join-Path $sourceRoot "lib\win32") -Destination (Join-Path $Destination "lib") -Recurse -Force
New-Item -ItemType Directory -Force -Path (Join-Path $Destination "bin") | Out-Null
Copy-Item -LiteralPath (Join-Path $sourceRoot "bin\win32") -Destination (Join-Path $Destination "bin") -Recurse -Force
Copy-Item -LiteralPath (Join-Path $sourceRoot "LICENSE") -Destination $Destination -Force

Assert-Hash $header $headerHash
Assert-Hash $library $libraryHash
Assert-Hash $dll $dllHash
Write-Host "OpenVR 2.15.6 dependency verified at $Destination"
