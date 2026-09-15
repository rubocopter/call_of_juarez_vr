param(
    [Parameter(Mandatory = $true)]
    [string]$TestExecutable,

    [Parameter(Mandatory = $true)]
    [string]$ProxyPath,

    [string]$SmokeArgument = "",
    [string]$WorkingRoot = ""
)

$ErrorActionPreference = "Stop"
$TestExecutable = [System.IO.Path]::GetFullPath($TestExecutable)
$ProxyPath = [System.IO.Path]::GetFullPath($ProxyPath)
if ([string]::IsNullOrWhiteSpace($WorkingRoot)) {
    $WorkingRoot = [System.IO.Path]::GetTempPath()
}
$WorkingRoot = [System.IO.Path]::GetFullPath($WorkingRoot)
$TestDirectory = Join-Path $WorkingRoot ("cojvr-proxy-smoke-" + [Guid]::NewGuid().ToString("N"))
$ResolvedRoot = $WorkingRoot.TrimEnd("\") + "\"
$ResolvedTestDirectory = [System.IO.Path]::GetFullPath($TestDirectory)
if (-not $ResolvedTestDirectory.StartsWith($ResolvedRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to use a proxy-smoke directory outside the requested working root."
}

try {
    New-Item -ItemType Directory -Path $TestDirectory -Force | Out-Null
    $IsolatedProxy = Join-Path $TestDirectory ([System.IO.Path]::GetFileName($ProxyPath))
    Copy-Item -LiteralPath $ProxyPath -Destination $IsolatedProxy

    $Arguments = @($IsolatedProxy)
    if (-not [string]::IsNullOrWhiteSpace($SmokeArgument)) {
        $Arguments += $SmokeArgument
    }
    & $TestExecutable @Arguments
    $ExitCode = $LASTEXITCODE
    if ($ExitCode -eq 0) {
        $LogPath = Join-Path $TestDirectory "cojvr.log"
        if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf) -or
            -not [bool]((Get-Content -LiteralPath $LogPath) -match
                'COJVR_EVENT .*"event":"run_end"')) {
            throw "Proxy smoke process exited normally without a structured run_end event."
        }
    }
} finally {
    if (Test-Path -LiteralPath $ResolvedTestDirectory -PathType Container) {
        Remove-Item -LiteralPath $ResolvedTestDirectory -Recurse -Force
    }
}

exit $ExitCode
