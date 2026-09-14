param(
    [string]$DestinationRoot = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($DestinationRoot)) {
    $DestinationRoot = Join-Path $PSScriptRoot "..\.cache\deps\OpenXR.Loader.1.1.63"
}

$Version = "1.1.63"
$ExpectedSha256 = "4E5A50A8807EF66F25180FF224E7D8150B594AA8EE4B07590F9ADE55A8E98703"
$Url = "https://github.com/KhronosGroup/OpenXR-SDK/releases/download/release-$Version/OpenXR.Loader.$Version.nupkg"
$DestinationRoot = [System.IO.Path]::GetFullPath($DestinationRoot)
$CacheRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.cache\downloads"))
$PackagePath = Join-Path $CacheRoot "OpenXR.Loader.$Version.nupkg"

$RequiredFiles = @(
    (Join-Path $DestinationRoot "include\openxr\openxr.h"),
    (Join-Path $DestinationRoot "include\openxr\openxr_platform.h"),
    (Join-Path $DestinationRoot "native\Win32\release\lib\openxr_loader.lib"),
    (Join-Path $DestinationRoot "native\Win32\release\bin\openxr_loader.dll")
)

if (($RequiredFiles | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) }).Count -eq 0) {
    Write-Host "OpenXR.Loader $Version is already bootstrapped at '$DestinationRoot'."
    exit 0
}

if (Test-Path -LiteralPath $DestinationRoot) {
    throw "OpenXR destination exists but is incomplete: '$DestinationRoot'. Remove that incomplete cache directory and rerun."
}

New-Item -ItemType Directory -Force -Path $CacheRoot | Out-Null

if (-not (Test-Path -LiteralPath $PackagePath -PathType Leaf)) {
    Write-Host "Downloading Khronos OpenXR.Loader $Version..."
    & curl.exe -L --fail --silent --show-error $Url -o $PackagePath
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to download '$Url'."
    }
}

$ActualSha256 = (Get-FileHash -LiteralPath $PackagePath -Algorithm SHA256).Hash.ToUpperInvariant()
if ($ActualSha256 -ne $ExpectedSha256) {
    throw "OpenXR.Loader package SHA-256 mismatch. Expected $ExpectedSha256, got $ActualSha256."
}

New-Item -ItemType Directory -Path $DestinationRoot | Out-Null
& tar.exe -xf $PackagePath -C $DestinationRoot
if ($LASTEXITCODE -ne 0) {
    throw "Failed to extract OpenXR.Loader package."
}

$Missing = @($RequiredFiles | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
if ($Missing.Count -ne 0) {
    throw "OpenXR.Loader package extraction is incomplete: $($Missing -join ', ')"
}

Write-Host "OpenXR.Loader $Version bootstrapped and SHA-256 verified."
