param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDirectory,

    [Parameter(Mandatory = $true)]
    [string]$BinaryDirectory
)

$ErrorActionPreference = "Stop"

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) {
        throw $Message
    }
}

function Write-Utf8Json([string]$Path, [object]$Value) {
    [System.IO.File]::WriteAllText(
        $Path,
        ($Value | ConvertTo-Json -Depth 10) + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))
}

$SourceDirectory = [System.IO.Path]::GetFullPath($SourceDirectory)
$BinaryDirectory = [System.IO.Path]::GetFullPath($BinaryDirectory)
$TestRoot = Join-Path $BinaryDirectory ("provenance-test-" + [Guid]::NewGuid().ToString("N"))
$ResolvedBinaryDirectory = $BinaryDirectory.TrimEnd("\") + "\"
$ResolvedTestRoot = [System.IO.Path]::GetFullPath($TestRoot)
if (-not $ResolvedTestRoot.StartsWith($ResolvedBinaryDirectory, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to use a test directory outside the configured binary directory."
}

try {
    New-Item -ItemType Directory -Path $TestRoot -Force | Out-Null
    $Scripts = @(
        "new_build_manifest.ps1",
        "collect_run_evidence.ps1",
        "get_run_provenance.ps1",
        "stage_d3d9_proxy.ps1",
        "stage_d3d9_openvr_flat.ps1"
    )
    foreach ($Script in $Scripts) {
        $Tokens = $null
        $Errors = $null
        [void][System.Management.Automation.Language.Parser]::ParseFile(
            (Join-Path $SourceDirectory "tools\$Script"),
            [ref]$Tokens,
            [ref]$Errors)
        Assert-True ($Errors.Count -eq 0) "$Script has PowerShell syntax errors."
    }
    $WrapperTokens = $null
    $WrapperErrors = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile(
        (Join-Path $SourceDirectory "tests\run_proxy_smoke.ps1"),
        [ref]$WrapperTokens,
        [ref]$WrapperErrors)
    Assert-True ($WrapperErrors.Count -eq 0) "run_proxy_smoke.ps1 has PowerShell syntax errors."

    $ArtifactDirectory = Join-Path $TestRoot "artifacts"
    New-Item -ItemType Directory -Path $ArtifactDirectory -Force | Out-Null
    $ProxyPath = Join-Path $ArtifactDirectory "d3d9_openvr_flat.dll"
    $OpenVrPath = Join-Path $ArtifactDirectory "openvr_api.dll"
    [System.IO.File]::WriteAllBytes($ProxyPath, [byte[]](0x43, 0x4F, 0x4A, 0x56, 0x52))
    [System.IO.File]::WriteAllBytes($OpenVrPath, [byte[]](0x4F, 0x50, 0x45, 0x4E, 0x56, 0x52))
    $BuildManifestPath = Join-Path $ArtifactDirectory "d3d9_openvr_flat.build-manifest.json"

    & (Join-Path $SourceDirectory "tools\new_build_manifest.ps1") `
        -RepositoryRoot $SourceDirectory `
        -Configuration Release `
        -DiagnosticMode d3d9_openvr_flat `
        -ProxyPath $ProxyPath `
        -OpenVrDllPath $OpenVrPath `
        -OutputPath $BuildManifestPath `
        -AllowDirty

    $BuildManifest = Get-Content -LiteralPath $BuildManifestPath -Raw | ConvertFrom-Json
    Assert-True ($BuildManifest.manifestType -eq "cojvr-build") "Build manifest type is missing."
    Assert-True ($BuildManifest.manifestId -match "^[A-F0-9]{64}$") "Build manifest ID is not a SHA-256."
    Assert-True ($BuildManifest.source.headCommit -match "^[a-f0-9]{40}$") "Source commit was not recorded."
    Assert-True ($BuildManifest.build.platform -eq "Win32") "Win32 build identity was not recorded."
    Assert-True (@($BuildManifest.artifacts).Count -eq 2) "Expected proxy and OpenVR artifacts."

    $GameDirectory = Join-Path $TestRoot "game"
    $RunId = "host-test-run"
    $RunDirectory = Join-Path $GameDirectory ".cojvr-evidence\runs\$RunId"
    New-Item -ItemType Directory -Path $RunDirectory -Force | Out-Null
    Copy-Item -LiteralPath $ProxyPath -Destination (Join-Path $GameDirectory "d3d9.dll")
    Copy-Item -LiteralPath $OpenVrPath -Destination (Join-Path $GameDirectory "openvr_api.dll")
    Copy-Item -LiteralPath $BuildManifestPath -Destination (Join-Path $RunDirectory "build-manifest.json")

    $ProxyHash = (Get-FileHash -LiteralPath (Join-Path $GameDirectory "d3d9.dll") -Algorithm SHA256).Hash
    $OpenVrHash = (Get-FileHash -LiteralPath (Join-Path $GameDirectory "openvr_api.dll") -Algorithm SHA256).Hash
    $BuildManifestHash = (Get-FileHash -LiteralPath $BuildManifestPath -Algorithm SHA256).Hash
    $RunManifest = [ordered]@{
        schemaVersion = 1
        manifestType = "cojvr-run"
        runId = $RunId
        buildManifestId = [string]$BuildManifest.manifestId
        buildManifestSha256 = $BuildManifestHash
        diagnosticMode = "d3d9_openvr_flat"
        validation = [ordered]@{ requireSteamOverlayAbsent = $true }
        deployment = @(
            [ordered]@{ role = "proxy"; destination = "d3d9.dll"; sha256 = $ProxyHash },
            [ordered]@{ role = "openvr_runtime"; destination = "openvr_api.dll"; sha256 = $OpenVrHash }
        )
    }
    Write-Utf8Json (Join-Path $GameDirectory ".cojvr-run.json") $RunManifest
    Write-Utf8Json (Join-Path $GameDirectory ".cojvr-d3d9-stage.json") ([ordered]@{
        runId = $RunId
        buildManifestId = [string]$BuildManifest.manifestId
        buildManifestSha256 = $BuildManifestHash
        diagnosticMode = "d3d9_openvr_flat"
    })
    Write-Utf8Json (Join-Path $GameDirectory ".cojvr-openvr-flat-stage.json") ([ordered]@{ runId = $RunId })
    Set-Content -LiteralPath (Join-Path $GameDirectory "cojvr.log") -Encoding UTF8 -Value @(
        "run_start: run_id=$RunId build_manifest_id=$($BuildManifest.manifestId) pid=123",
        "diagnostic event"
    )

    $Provenance = & (Join-Path $SourceDirectory "tools\get_run_provenance.ps1") `
        -GameDirectory $GameDirectory `
        -ExpectedDiagnosticMode "d3d9_openvr_flat"
    Assert-True ($Provenance.RunId -eq $RunId) "Verifier did not return the bound run ID."
    Assert-True ([bool]$Provenance.Run.validation.requireSteamOverlayAbsent) `
        "Run validation requirements were not preserved by provenance."

    $PackagePath = Join-Path $TestRoot "$RunId.zip"
    & (Join-Path $SourceDirectory "tools\collect_run_evidence.ps1") `
        -GameDirectory $GameDirectory `
        -OutputPath $PackagePath
    Assert-True (Test-Path -LiteralPath $PackagePath -PathType Leaf) "Evidence package was not created."

    $EvidenceManifestPath = Join-Path $RunDirectory "evidence-manifest.json"
    $EvidenceManifest = Get-Content -LiteralPath $EvidenceManifestPath -Raw | ConvertFrom-Json
    Assert-True ([bool]$EvidenceManifest.runtimeStarted) "Run start was not correlated."
    Assert-True ([bool]$EvidenceManifest.incomplete) "Missing run_end was not marked incomplete."
    Assert-True (-not (@($EvidenceManifest.deploymentChecks) | Where-Object { -not $_.matches })) `
        "Matching deployed artifacts were not verified."

    Add-Content -LiteralPath (Join-Path $GameDirectory "cojvr.log") -Encoding UTF8 -Value "run_end: run_id=$RunId"
    & (Join-Path $SourceDirectory "tools\collect_run_evidence.ps1") `
        -GameDirectory $GameDirectory `
        -OutputPath $PackagePath
    $EvidenceManifest = Get-Content -LiteralPath $EvidenceManifestPath -Raw | ConvertFrom-Json
    Assert-True ([bool]$EvidenceManifest.runtimeEnded) "Run end was not correlated."
    Assert-True (-not [bool]$EvidenceManifest.incomplete) "Finalized run was marked incomplete."

    [System.IO.File]::WriteAllBytes((Join-Path $GameDirectory "d3d9.dll"), [byte[]](0x00))
    & (Join-Path $SourceDirectory "tools\collect_run_evidence.ps1") `
        -GameDirectory $GameDirectory `
        -OutputPath $PackagePath
    $EvidenceManifest = Get-Content -LiteralPath $EvidenceManifestPath -Raw | ConvertFrom-Json
    $ProxyCheck = @($EvidenceManifest.deploymentChecks | Where-Object { $_.destination -eq "d3d9.dll" })
    Assert-True ($ProxyCheck.Count -eq 1 -and -not [bool]$ProxyCheck[0].matches) `
        "Changed deployed proxy was not reported."

    Write-Host "PASS - provenance build/run manifests and evidence collection"
} finally {
    if (Test-Path -LiteralPath $ResolvedTestRoot -PathType Container) {
        Remove-Item -LiteralPath $ResolvedTestRoot -Recurse -Force
    }
}
