# Launcher-owned startup preferences only. Dot-sourcing performs no I/O.
# This helper neither applies a profile nor accesses game/video/runtime settings.
function Get-CoJResolutionSettingsPath {
    if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        throw 'LOCALAPPDATA is unavailable.'
    }
    return (Join-Path $env:LOCALAPPDATA 'CoJVR/CallOfJuarez/resolution.json')
}

function ConvertTo-CoJResolutionSettingsDimension {
    param([object]$Value)
    # Keep this independent of the video workflow, with the same no-rounding rule.
    if ($null -eq $Value -or @('String','Byte','SByte','Int16','UInt16','Int32','UInt32','Int64','UInt64') -cnotcontains
        [Type]::GetTypeCode($Value.GetType()).ToString() -or [string]$Value -cnotmatch '\A[0-9]+\z') {
        throw 'Resolution dimensions must be explicit nonnegative integers, without rounding.'
    }
    $Parsed = 0
    if (-not [int]::TryParse([string]$Value, [ref]$Parsed)) {
        throw 'Resolution dimension exceeds the native integer range.'
    }
    return $Parsed
}

function New-CoJResolutionSettings {
    param([string]$Profile, [object]$Width, [object]$Height)
    if (@('Preserve','Auto','75%','100%','125%','Manual') -cnotcontains $Profile) {
        throw 'Unknown resolution profile.'
    }
    $W = ConvertTo-CoJResolutionSettingsDimension $Width
    $H = ConvertTo-CoJResolutionSettingsDimension $Height
    if ($Profile -ceq 'Manual') {
        if ($W -eq 0 -or $H -eq 0) { throw 'Manual resolution requires both positive dimensions.' }
    } elseif ($W -ne 0 -or $H -ne 0) {
        throw 'Only Manual accepts nonzero resolution dimensions.'
    }
    return [pscustomobject][ordered]@{schemaVersion=1; profile=$Profile; width=$W; height=$H}
}

function Assert-CoJResolutionSettingsPath {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { throw 'A settings file path is required.' }
    $FullPath = [IO.Path]::GetFullPath($Path)
    $Root = [IO.Path]::GetPathRoot($FullPath)
    if ($FullPath.Substring($Root.Length).Contains(':')) { throw 'Alternate data streams are not settings files.' }
    # Check the leaf AND every existing ancestor, even when the leaf is missing.
    $Probe = $FullPath
    while ($null -ne $Probe) {
        try {
            $Attributes = [IO.File]::GetAttributes($Probe)
            if ($Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw 'Resolution settings paths must not contain reparse points.'
            }
            if ($Probe -ceq $FullPath -and ($Attributes -band [IO.FileAttributes]::Directory)) {
                throw 'Resolution settings path must be a file.'
            }
        } catch [IO.FileNotFoundException] {
            # A missing file/ancestor is valid; permissions and other errors reject.
        } catch [IO.DirectoryNotFoundException] {
        }
        $Probe = [IO.Path]::GetDirectoryName($Probe)
    }
    return $FullPath
}

function Read-CoJResolutionSettings {
    param([Parameter(Mandatory=$true)][string]$Path)
    $FullPath = Assert-CoJResolutionSettingsPath $Path
    try {
        $Stream = [IO.File]::Open($FullPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    } catch [IO.FileNotFoundException] {
        return (New-CoJResolutionSettings Preserve 0 0)
    } catch [IO.DirectoryNotFoundException] {
        return (New-CoJResolutionSettings Preserve 0 0)
    }
    try {
        if ($Stream.Length -gt 4096) { throw 'Resolution settings exceed 4096 bytes.' }
        $Bytes = New-Object byte[] ([int]$Stream.Length)
        $Count = 0
        while ($Count -lt $Bytes.Length) {
            $Read = $Stream.Read($Bytes, $Count, $Bytes.Length - $Count)
            if ($Read -eq 0) { throw 'Incomplete resolution settings read.' }
            $Count += $Read
        }
        $Utf8 = New-Object Text.UTF8Encoding($false, $true)
        $Json = $Utf8.GetString($Bytes)
        if ($Json.Length -gt 0 -and $Json[0] -eq [char]0xFEFF) { $Json = $Json.Substring(1) }
    } finally { $Stream.Dispose() }

    # Deliberately small canonical flat JSON grammar, also on Windows PS 5.1.
    # No coercion, escaped keys, comments, nested values or duplicate-key collapse.
    $Space = '[\x20\t\r\n]*'
    $Member = '"(?<key>[A-Za-z]+)"' + $Space + ':' + $Space +
        '(?<value>"[^"\\\x00-\x1f]*"|0|[1-9][0-9]*)'
    $Match = [regex]::Match($Json, '\A' + $Space + '\{' + $Space + $Member +
        '(?:' + $Space + ',' + $Space + $Member + ')*' + $Space + '\}' + $Space + '\z')
    if (-not $Match.Success) { throw 'Malformed canonical resolution settings JSON.' }
    $Fields = @{}
    for ($Index = 0; $Index -lt $Match.Groups['key'].Captures.Count; $Index++) {
        $Key = $Match.Groups['key'].Captures[$Index].Value
        if (@('schemaVersion','profile','width','height') -cnotcontains $Key -or $Fields.ContainsKey($Key)) {
            throw 'Unknown or duplicate resolution settings key.'
        }
        $Fields[$Key] = $Match.Groups['value'].Captures[$Index].Value
    }
    if ($Fields.Count -ne 4 -or $Fields['schemaVersion'] -cne '1' -or
        $Fields['profile'] -cnotmatch '\A"(?:Preserve|Auto|75%|100%|125%|Manual)"\z') {
        throw 'Incomplete or unsupported resolution settings.'
    }
    return (New-CoJResolutionSettings $Fields['profile'].Trim('"') $Fields['width'] $Fields['height'])
}

function Write-CoJResolutionSettings {
    param(
        [Parameter(Mandatory=$true)][string]$Path,
        [Parameter(Mandatory=$true)][string]$Profile,
        [object]$Width = 0,
        [object]$Height = 0
    )
    $Settings = New-CoJResolutionSettings $Profile $Width $Height
    $FullPath = Assert-CoJResolutionSettingsPath $Path
    $Directory = [IO.Path]::GetDirectoryName($FullPath)
    [void][IO.Directory]::CreateDirectory($Directory)
    $FullPath = Assert-CoJResolutionSettingsPath $FullPath
    $Temporary = Join-Path $Directory ('.cojvr-resolution-' + [guid]::NewGuid().ToString('N') + '.tmp')
    $OwnedTemporary = $false
    try {
        $Stream = [IO.File]::Open($Temporary, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        $OwnedTemporary = $true
        try {
            $Utf8 = New-Object Text.UTF8Encoding($false)
            $Bytes = $Utf8.GetBytes(($Settings | ConvertTo-Json -Compress))
            $Stream.Write($Bytes, 0, $Bytes.Length)
            $Stream.Flush($true)
        } finally { $Stream.Dispose() }
        $FullPath = Assert-CoJResolutionSettingsPath $FullPath
        if ([IO.File]::Exists($FullPath)) {
            [IO.File]::Replace($Temporary, $FullPath, [NullString]::Value)
        } else {
            # Move does not overwrite a concurrently created destination.
            [IO.File]::Move($Temporary, $FullPath)
        }
        $OwnedTemporary = $false
    } finally {
        if ($OwnedTemporary) { [IO.File]::Delete($Temporary) }
    }
    return $Settings
}
