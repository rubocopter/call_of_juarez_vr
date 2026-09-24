param(
    [Parameter(Position = 0, Mandatory = $true)]
    [ValidateSet("prepare", "finish")]
    [string]$Action,
    [string]$GameDirectory = "",
    [switch]$ReachedGameplay
)

$ErrorActionPreference = "Stop"
$Root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if ([string]::IsNullOrWhiteSpace($GameDirectory)) {
    $Settings = Get-Content -LiteralPath (Join-Path $Root "work/vr-test.json") -Raw | ConvertFrom-Json
    $GameDirectory = [string]$Settings.gameDirectory
}
$GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
$Dll = Join-Path $Root "build-win32/Release/d3d9_pool_probe.dll"
$Manifest = Join-Path $Root "build-win32/Release/d3d9_pool_probe.build-manifest.json"
$State = Join-Path $GameDirectory ".cojvr-d3d9-stage.json"
$Capture = Join-Path $GameDirectory "cojvr-d3d9-pool-probe.csv"

if ($Action -eq "prepare") {
    if (Get-Process -Name CoJ -ErrorAction SilentlyContinue) {
        throw "Close Call of Juarez before preparing the pool probe."
    }
    & cmake --build (Join-Path $Root "build-win32") --config Release --target cojvr_d3d9_pool_probe cojvr_d3d9_pool_probe_smoke
    if ($LASTEXITCODE -ne 0) { throw "Pool probe build failed." }
    & ctest --test-dir (Join-Path $Root "build-win32") -C Release -R '^d3d9_pool_probe_smoke$' --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Pool probe smoke test failed." }
    & (Join-Path $PSScriptRoot "new_build_manifest.ps1") `
        -RepositoryRoot $Root -Configuration Release -DiagnosticMode d3d9_pool_probe `
        -ProxyPath $Dll -OutputPath $Manifest -AllowDirty
    if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) { throw "Build manifest creation failed." }
    & (Join-Path $PSScriptRoot "stage_d3d9_proxy.ps1") `
        -GameDirectory $GameDirectory -ProxyPath $Dll -BuildManifestPath $Manifest `
        -ValidationProfile performance
    if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) { throw "Pool probe staging failed." }
    Write-Host "Play manually from menu into gameplay, then close the game normally. Run this script with 'finish -ReachedGameplay' afterward."
    return
}

if (Get-Process -Name CoJ -ErrorAction SilentlyContinue) {
    throw "Close Call of Juarez normally before finishing the pool probe."
}
if (-not (Test-Path -LiteralPath $State -PathType Leaf)) {
    throw "No staged pool probe was found."
}
$Stage = Get-Content -LiteralPath $State -Raw | ConvertFrom-Json
if ([string]$Stage.diagnosticMode -ne "d3d9_pool_probe") {
    throw "Staged diagnostic mode is '$($Stage.diagnosticMode)', not d3d9_pool_probe."
}
$RunId = [string]$Stage.runId
$RunManifest = Join-Path (Join-Path (Join-Path $GameDirectory ".cojvr-evidence/runs") $RunId) "run-manifest.json"
$Out = Join-Path $Root ("work/evidence/d3d9_pool_probe/" + $RunId)
New-Item -ItemType Directory -Path $Out -Force | Out-Null
if (Test-Path -LiteralPath $RunManifest -PathType Leaf) {
    Copy-Item -LiteralPath $RunManifest -Destination (Join-Path $Out "run-manifest.json")
}
if (Test-Path -LiteralPath $Capture -PathType Leaf) {
    Copy-Item -LiteralPath $Capture -Destination (Join-Path $Out "resource-creations.csv")
}

& (Join-Path $PSScriptRoot "unstage_d3d9_proxy.ps1") -GameDirectory $GameDirectory
if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) { throw "Unstage failed; inspect deployment state." }
if (-not (Test-Path -LiteralPath (Join-Path $Out "resource-creations.csv"))) {
    throw "No resource-creation capture was produced; staged proxy has been restored."
}
& python (Join-Path $PSScriptRoot "analyze_d3d9_pool_probe.py") `
    (Join-Path $Out "resource-creations.csv") `
    --run-manifest (Join-Path $Out "run-manifest.json") `
    --output (Join-Path $Out "RESULTS.md")
if ($LASTEXITCODE -ne 0) { throw "Pool probe analysis failed; raw evidence remains in '$Out'." }
Add-Content -LiteralPath (Join-Path $Out "RESULTS.md") -Value `
    ("Gameplay reached: " + $(if ($ReachedGameplay) { "operator-confirmed" } else { "unconfirmed" }) + ".")
Write-Host "Pool probe evidence and report: $Out"
