param([string]$Destination = "")
$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$revision = "9981c6350dc58c0ce21f024fd65e31e851c31689"
if ([string]::IsNullOrWhiteSpace($Destination)) {
    $Destination = Join-Path $repoRoot ".cache\deps\nvidia-dlss"
}
$Destination = [IO.Path]::GetFullPath($Destination)
function Invoke-Git([string[]]$GitArguments) {
    & git @GitArguments
    if ($LASTEXITCODE -ne 0) { throw "DLSS SDK git operation failed" }
}
if (-not (Test-Path -LiteralPath $Destination)) {
    Invoke-Git @("clone", "--no-checkout", "--filter=blob:none", "--sparse", "https://github.com/NVIDIA/DLSS.git", $Destination)
    Invoke-Git @("-C", $Destination, "sparse-checkout", "set", "include", "lib/Windows_x86_64/rel", "lib/Windows_x86_64/x64")
    Invoke-Git @("-C", $Destination, "checkout", "--detach", $revision)
}
# Existing folders are verified, never reset, cleaned or overwritten.
$actualRevision = & git -C $Destination rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actualRevision -ne $revision) { throw "Expected the pinned NVIDIA DLSS SDK revision $revision" }
$changes = & git -C $Destination status --porcelain --untracked-files=normal
if ($LASTEXITCODE -ne 0 -or $changes) { throw "Existing NVIDIA SDK contains changes; preserving it" }
$hashes = @{
    "include/nvsdk_ngx.h" = "96F21266A05EF06BB49F58C89DE1A7845F2D9B1E0E0E069FE03740B8EAF0344D"
    "lib/Windows_x86_64/x64/nvsdk_ngx_d.lib" = "BC43DF9EC032004FDE681D3E3AFD01CDE239E03A0125B822E88674C84287F99C"
    "lib/Windows_x86_64/rel/nvngx_dlss.dll" = "D00B5B0AB29C3CA101B70C3A5ECB3A01B7EF33766A3383A28A18623083E6123E"
}
foreach ($relative in $hashes.Keys) {
    $file = Join-Path $Destination $relative
    if (-not (Test-Path -LiteralPath $file -PathType Leaf) -or
        (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $hashes[$relative]) {
        throw "Pinned DLSS SDK file unavailable or SHA-256 mismatch: $relative"
    }
}
$runtime = Join-Path $Destination "lib/Windows_x86_64/rel/nvngx_dlss.dll"
if ((Get-AuthenticodeSignature -LiteralPath $runtime).Status -ne "Valid") {
    throw "NVIDIA runtime Authenticode verification failed"
}
Write-Host "Verified official NVIDIA DLSS SDK $revision; license: $Destination\LICENSE.txt"
Write-Host "Build experimental helper: cmake -S tools/dlss_host -B build/dlss-host-x64 -A x64"
# No game deployment, release packaging or machine-wide install.
