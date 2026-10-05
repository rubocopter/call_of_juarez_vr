param(
    [Parameter(Mandatory = $true)] [string]$SourceDirectory,
    [Parameter(Mandatory = $true)] [string]$BinaryDirectory
)
$ErrorActionPreference = 'Stop'
$BinaryRoot = [IO.Path]::GetFullPath($BinaryDirectory).TrimEnd('\') + '\'
$TestRoot = Join-Path $BinaryRoot ('bootstrap-openvr-' + [guid]::NewGuid().ToString('N'))
$Download = Join-Path $TestRoot '.cache/downloads'
$ScriptRoot = Join-Path $TestRoot 'tools'
New-Item -ItemType Directory -Path $Download, $ScriptRoot -Force | Out-Null
$Succeeded = $false
try {
    $Archive = Join-Path $SourceDirectory '.cache/downloads/openvr-v2.15.6.zip'
    if ((Get-FileHash $Archive -Algorithm SHA256).Hash -ne '7629E6586338DBC0F877571870F04534E24EE752126606C5D8FFDD7C3E7D31D4') { throw 'Test needs the pinned OpenVR ZIP.' }
    Copy-Item -LiteralPath $Archive -Destination $Download
    Copy-Item -LiteralPath (Join-Path $SourceDirectory 'tools/bootstrap_openvr.ps1') -Destination $ScriptRoot
    $Bootstrap = Join-Path $ScriptRoot 'bootstrap_openvr.ps1'
    $Destination = Join-Path $TestRoot 'sdk'
    for ($Attempt = 0; $Attempt -lt 2; ++$Attempt) {
        & (Get-Process -Id $PID).Path -NoProfile -File $Bootstrap -Destination $Destination
        if ($LASTEXITCODE -ne 0) { throw 'Bootstrap rejected the original pinned ZIP/header.' }
        foreach ($Pair in @(
            @('headers/openvr.h', '1E6ED57199896CC1F7C5484E50FA18955E97BE15BE690BEB28D998C877EAD7FD'),
            @('lib/win32/openvr_api.lib', '9EC5921313D97EB88D7FB98371E31CC0EB73F792266F8EA4EF387031FE0D3A3D'),
            @('bin/win32/openvr_api.dll', 'AB696E4F218A95B3E396BC310F9FE6485DF48C99C0969762083212B1E1F025A6')
        )) {
            if ((Get-FileHash (Join-Path $Destination $Pair[0]) -Algorithm SHA256).Hash -ne $Pair[1]) { throw "Wrong SDK bytes: $($Pair[0])" }
        }
    }
    $Succeeded = $true
    Write-Host 'PASS - pinned offline OpenVR extraction and repeated verification.'
} finally {
    if ($Succeeded -and ([IO.Path]::GetFullPath($TestRoot)).StartsWith($BinaryRoot, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $TestRoot -Recurse -Force
    } else { Write-Host "Retained bootstrap fixture: $TestRoot" }
}
