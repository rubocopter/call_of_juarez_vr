param([Parameter(Mandatory=$true)][string]$FixturePath)
$ErrorActionPreference = 'Stop'
$baseline = & $FixturePath baseline
$baselineExit = $LASTEXITCODE
if ($baselineExit -eq 77) { Write-Host '64-bit Windows is required for capacity fixture.'; exit 77 }
if ($baselineExit -ne 0) { throw "Baseline fixture failed: $baseline" }
$root = Join-Path ([IO.Path]::GetTempPath()) ('cojvr-address-space-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
try {
    $bytes = [IO.File]::ReadAllBytes($FixturePath)
    $pe = [BitConverter]::ToInt32($bytes, 0x3c)
    if ([BitConverter]::ToUInt16($bytes, $pe + 4) -ne 0x14c -or ($bytes[$pe + 22] -band 0x20)) {
        throw 'Fixture must be x86 with LARGEADDRESSAWARE:NO.'
    }
    # Synthetic host fixture only. The production helper accepts only exact CoJ.
    $bytes[$pe + 22] = $bytes[$pe + 22] -bor 0x20
    $extended = Join-Path $root 'extended.exe'
    [IO.File]::WriteAllBytes($extended, $bytes)
    & $extended extended
    if ($LASTEXITCODE -ne 0) { throw 'Header-only extended-capacity fixture failed.' }
    Write-Host $baseline
} finally {
    Remove-Item -LiteralPath (Join-Path $root 'extended.exe') -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $root -Force
}
