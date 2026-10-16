# OpenVR action/default-binding inventory shared by build, stage and restoration.
function Assert-CojvrOpenVrInputManifest([object]$Manifest) {
    if ([int]$Manifest.schemaVersion -ne 1 -or @($Manifest.files).Count -lt 2) {
        throw "Unsupported or incomplete OpenVR input inventory."
    }
    $Seen = @{}
    foreach ($File in @($Manifest.files)) {
        $Relative = [string]$File.path
        if (($Relative -cne 'actions.json' -and
             $Relative -cnotmatch '^bindings/(?:[A-Za-z0-9_-]+/)*[A-Za-z0-9_-]+\.json$') -or
            @($Relative.Split('/') | Where-Object { $_ -match '^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)' }).Count -gt 0 -or
            $Seen.ContainsKey($Relative) -or [string]$File.sha256 -notmatch '^[A-Fa-f0-9]{64}$') {
            throw "Unsafe, duplicate or invalid OpenVR input inventory path/hash '$Relative'."
        }
        $Seen[$Relative] = $true
    }
    if (-not $Seen.ContainsKey('actions.json')) { throw 'OpenVR input inventory is missing actions.json.' }
}

function Get-CojvrOpenVrInputManifest([string]$Directory) {
    $Directory = [IO.Path]::GetFullPath($Directory)
    $ActionPath = Join-Path $Directory 'actions.json'
    # Refuse reparse points, including parent directories, before reading inputs.
    function Assert-InputFile([string]$Path) {
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing OpenVR input artifact '$Path'." }
        $Item = Get-Item -LiteralPath $Path -Force
        while ($null -ne $Item) {
            if ($Item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "OpenVR input reparse point '$($Item.FullName)' is not allowed." }
            if ($Item -is [IO.FileInfo]) { $Item = $Item.Directory } else { $Item = $Item.Parent }
        }
    }
    Assert-InputFile $ActionPath
    $Actions = Get-Content -LiteralPath $ActionPath -Encoding UTF8 -Raw | ConvertFrom-Json
    if (@($Actions.default_bindings).Count -eq 0) { throw 'OpenVR action manifest has no default bindings.' }
    $Files = @([ordered]@{path='actions.json'; sha256=(Get-FileHash -LiteralPath $ActionPath -Algorithm SHA256).Hash.ToUpperInvariant()})
    $Controllers = @{}; $Paths = @{}
    foreach ($Binding in @($Actions.default_bindings)) {
        $Relative = [string]$Binding.binding_url; $Controller = [string]$Binding.controller_type
        if ($Relative -cnotmatch '^bindings/(?:[A-Za-z0-9_-]+/)*[A-Za-z0-9_-]+\.json$' -or
            $Controller -cnotmatch '^[a-z0-9_]+$' -or $Controllers.ContainsKey($Controller) -or $Paths.ContainsKey($Relative)) {
            throw "Unsafe or duplicate OpenVR default binding '$Relative' / '$Controller'."
        }
        $Controllers[$Controller] = $true; $Paths[$Relative] = $true
        # Validate Windows device names before resolving or hashing any binding.
        $Probe = @{schemaVersion=1; files=@($Files[0], @{path=$Relative; sha256=('0' * 64)})}
        Assert-CojvrOpenVrInputManifest $Probe
        $Path = Join-Path $Directory $Relative
        Assert-InputFile $Path
        $Config = Get-Content -LiteralPath $Path -Encoding UTF8 -Raw | ConvertFrom-Json
        if ([string]$Config.controller_type -cne $Controller) { throw "OpenVR binding controller mismatch at '$Relative'." }
        $Files += [ordered]@{path=$Relative; sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()}
    }
    $Result = [ordered]@{schemaVersion=1; files=@($Files | Sort-Object { [string]$_.path })}
    Assert-CojvrOpenVrInputManifest $Result
    return $Result
}

