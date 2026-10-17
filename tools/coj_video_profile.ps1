# Exact CoJ Video.scr policy. Paths are supplied by the physical workflow;
# importing this helper has no filesystem or staging side effects.
function ConvertTo-CoJNativeDimension {
    param([object]$Value)
    if ($null -eq $Value -or $Value -is [bool] -or $Value -is [double] -or
        $Value -is [single] -or $Value -is [decimal] -or
        [string]$Value -notmatch '^[0-9]+$') {
        throw 'Resolution dimensions must be explicit nonnegative integers, without rounding.'
    }
    $Parsed=0
    if (-not [int]::TryParse([string]$Value,[ref]$Parsed) -or $Parsed -lt 0) {
        throw 'Resolution dimension exceeds the native integer range.'
    }
    return $Parsed
}
function Apply-CoJVrVideoProfile {
    param(
        [Parameter(Mandatory = $true)] [string]$VideoPath,
        [Parameter(Mandatory = $true)] [string]$StatePath,
        [Parameter(Mandatory = $true)] [string]$BackupPath,
        [ValidateSet('preserve', '2804x2860')] [string]$NativeResolution = 'preserve',
        [object]$ExperimentalWidth = 0,
        [object]$ExperimentalHeight = 0
    )

    # Explicit startup experiments only: integer representation is checked by
    # strict parsing, never rounded/clamped. This is not native mode/memory preflight
    # or release qualification; the caller owns those gates and startup policy.
    $ExperimentalWidth=ConvertTo-CoJNativeDimension $ExperimentalWidth
    $ExperimentalHeight=ConvertTo-CoJNativeDimension $ExperimentalHeight
    $Experimental = $ExperimentalWidth -ne 0 -or $ExperimentalHeight -ne 0
    if ($Experimental -and ($ExperimentalWidth -le 0 -or $ExperimentalHeight -le 0)) {
        throw 'An experimental startup extent requires both ExperimentalWidth and ExperimentalHeight to be positive native integers.'
    }
    if ($Experimental -and $NativeResolution -ne 'preserve') {
        throw 'Experimental startup dimensions cannot be combined with a fixed NativeResolution override.'
    }
    if ((Test-Path -LiteralPath $StatePath) -or (Test-Path -LiteralPath $BackupPath)) {
        throw 'A VR video profile or recovery backup already exists. Restore it before preparing another candidate.'
    }
    if (-not (Test-Path -LiteralPath $VideoPath -PathType Leaf)) {
        throw "Call of Juarez Video.scr was not found at '$VideoPath'."
    }

    $Text = [IO.File]::ReadAllText($VideoPath)
    # Count active declarations separately so a malformed duplicate cannot hide
    # beside one valid setting. ! declarations and // comments are not active.
    $ResolutionPattern = '(?im)^[\t ]*Resolution\([\t ]*(?<width>[0-9]+)[\t ]*,[\t ]*(?<height>[0-9]+)[\t ]*\)[\t ]*(?://[^\r\n]*)?\r?$'
    $FsaaPattern = '(?im)^[\t ]*FSAA\([\t ]*(?<value>[0-9]+)[\t ]*\)[\t ]*(?://[^\r\n]*)?\r?$'
    $Resolution = [regex]::Matches($Text, $ResolutionPattern)
    $Fsaa = [regex]::Matches($Text, $FsaaPattern)
    if ([regex]::Matches($Text, '(?im)^[\t ]*Resolution\b').Count -ne 1 -or
        [regex]::Matches($Text, '(?im)^[\t ]*FSAA\b').Count -ne 1 -or
        $Resolution.Count -ne 1 -or $Fsaa.Count -ne 1) {
        throw 'Video.scr must contain exactly one valid Resolution(...) and FSAA(...) setting.'
    }
    $Width = 0
    $Height = 0
    $FsaaValue = 0
    if (-not [int]::TryParse($Resolution[0].Groups['width'].Value, [ref]$Width) -or
        -not [int]::TryParse($Resolution[0].Groups['height'].Value, [ref]$Height) -or
        $Width -le 0 -or $Height -le 0 -or
        -not [int]::TryParse($Fsaa[0].Groups['value'].Value, [ref]$FsaaValue)) {
        throw 'Video.scr resolution must have positive integer dimensions and FSAA must be a nonnegative integer.'
    }

    $OriginalResolution = "${Width}x${Height}"
    # Default preserves the selected extent. The explicit native-size trial
    # changes engine video settings, including menus/theater, never HMD scale.
    # Windowed presentation does not require a matching monitor display mode.
    $Value = $Fsaa[0].Groups['value']
    $Updated = $Text.Substring(0, $Value.Index) + '0' + $Text.Substring($Value.Index + $Value.Length)
    if ($Experimental -or $NativeResolution -ne 'preserve') {
        $FullscreenPattern = '(?im)^[\t ]*Fullscreen\([\t ]*\)[\t ]*(?://[^\r\n]*)?\r?$'
        if ([regex]::Matches($Text, '(?im)^[\t ]*Fullscreen\b').Count -ne
            [regex]::Matches($Text, $FullscreenPattern).Count -or
            [regex]::Matches($Text, $FullscreenPattern).Count -gt 1) {
            throw 'Video.scr contains an ambiguous Fullscreen setting.'
        }
        $Width = if ($Experimental) { $ExperimentalWidth } else { 2804 }
        $Height = if ($Experimental) { $ExperimentalHeight } else { 2860 }
        $Updated = [regex]::Replace($Updated, '(?im)^([\t ]*Resolution\()[\t ]*[0-9]+[\t ]*,[\t ]*[0-9]+[\t ]*(\))', ('${1}' + "$Width,$Height" + '${2}'))
        $Updated = [regex]::Replace($Updated, '(?im)^([\t ]*)(Fullscreen\()', '${1}// VR native-resolution trial: ${2}')
    }
    $Encoding = [Text.UTF8Encoding]::new($false)
    $AppliedBytes = if ($Updated -eq $Text) { [IO.File]::ReadAllBytes($VideoPath) } else { $Encoding.GetBytes($Updated) }
    $Hasher = [Security.Cryptography.SHA256]::Create()
    try { $AppliedHash = [BitConverter]::ToString($Hasher.ComputeHash($AppliedBytes)).Replace('-', '') } finally { $Hasher.Dispose() }

    Copy-Item -LiteralPath $VideoPath -Destination $BackupPath
    $OriginalHash = (Get-FileHash -LiteralPath $BackupPath -Algorithm SHA256).Hash.ToUpperInvariant()
    $State = [ordered]@{
        schemaVersion = 1
        videoPath = $VideoPath
        originalSha256 = $OriginalHash
        appliedSha256 = $AppliedHash
        resolution = "${Width}x${Height}"
        originalResolution = $OriginalResolution
        nativeResolution = if ($Experimental) { "${Width}x${Height}" } else { $NativeResolution }
        # Additive schema-1 intent fields; zero requested axes mean no explicit
        # experiment. resolution/appliedSha256 remain the actual applied profile.
        experimental = [bool]$Experimental
        requestedWidth = $ExperimentalWidth
        requestedHeight = $ExperimentalHeight
        windowed = -not [regex]::IsMatch($Updated, '(?im)^[\t ]*Fullscreen\b')
        fsaa = 0
    }
    try {
        # Persist recovery before mutation so an interrupted apply is restorable.
        [IO.File]::WriteAllText($StatePath, ($State | ConvertTo-Json) + [Environment]::NewLine, $Encoding)
        if ($Updated -ne $Text) { [IO.File]::WriteAllBytes($VideoPath, $AppliedBytes) }
        if ((Get-FileHash -LiteralPath $VideoPath -Algorithm SHA256).Hash -ne $AppliedHash) {
            throw 'Video.scr does not match the recorded applied profile.'
        }
        Write-Host "Applied reversible VR video profile: $($State.resolution), FSAA 0, native override=$($State.nativeResolution), experimental=$Experimental, windowed=$($State.windowed)."
        return [pscustomobject]$State
    } catch {
        $ApplyError = $_
        # Retain the journal and backup if rollback fails; finish can retry.
        try {
            if (Test-Path -LiteralPath $StatePath -PathType Leaf) {
                [void](Restore-CoJVrVideoProfile -StatePath $StatePath -BackupPath $BackupPath)
            } else {
                Remove-Item -LiteralPath $BackupPath -Force
            }
        } catch {
            throw "Video profile apply failed: $($ApplyError.Exception.Message) Rollback failed; recovery evidence retained: $($_.Exception.Message)"
        }
        throw $ApplyError
    }
}

function Restore-CoJVrVideoProfile {
    param(
        [Parameter(Mandatory = $true)] [string]$StatePath,
        [Parameter(Mandatory = $true)] [string]$BackupPath
    )

    if (-not (Test-Path -LiteralPath $StatePath -PathType Leaf)) { return $false }
    $State = Get-Content -LiteralPath $StatePath -Encoding UTF8 -Raw | ConvertFrom-Json
    $VideoPath = [string]$State.videoPath
    if ([string]::IsNullOrWhiteSpace($VideoPath) -or
        [string]$State.originalSha256 -notmatch '^[0-9a-fA-F]{64}$' -or
        [string]$State.appliedSha256 -notmatch '^[0-9a-fA-F]{64}$') {
        throw 'The VR video-profile recovery journal is invalid.'
    }
    $OriginalHash = ([string]$State.originalSha256).ToUpperInvariant()
    # Optional schema-1 conflicts entries are retained recovery evidence, each
    # with path and sha256. Older journals omit this field. Once journaled, every
    # copy must verify even on cleanup retries; never recreate a missing/bad copy.
    foreach ($Conflict in @($State.conflicts)) {
        if ($null -eq $Conflict) { continue }
        if ([string]::IsNullOrWhiteSpace([string]$Conflict.path) -or
            [string]$Conflict.sha256 -notmatch '^[0-9a-fA-F]{64}$' -or
            -not (Test-Path -LiteralPath $Conflict.path -PathType Leaf) -or
            (Get-FileHash -LiteralPath $Conflict.path -Algorithm SHA256).Hash -ne [string]$Conflict.sha256) {
            throw 'A journaled VR video-profile conflict copy is missing or damaged; recovery evidence retained.'
        }
    }

    $Destination = $null
    $Backup = $null
    try {
        # A read-only, write/delete-denying lease preserves already-original
        # cleanup even with read-only settings or an unused missing backup.
        $Missing = -not [IO.File]::Exists($VideoPath)
        if (-not $Missing) {
            $Destination = [IO.File]::Open($VideoPath, 'Open', 'Read', 'Read')
            $CurrentHash = Get-CoJVrVideoStreamHash $Destination
            if ($CurrentHash -ne $OriginalHash) {
                $Destination.Dispose()
                $Destination = $null
                # Re-read after upgrading ownership; no earlier hash authorizes
                # mutation of bytes that could change during the upgrade.
                $Destination = [IO.File]::Open($VideoPath, 'Open', 'ReadWrite', 'None')
                $CurrentHash = Get-CoJVrVideoStreamHash $Destination
            }
        } else { $CurrentHash = $null }
        if ($CurrentHash -ne $OriginalHash) {
        if (-not (Test-Path -LiteralPath $BackupPath -PathType Leaf)) {
            throw "The VR video-profile backup is missing; refusing to overwrite '$VideoPath'."
        }
        $Backup = [IO.File]::Open($BackupPath, 'Open', 'Read', 'Read')
        if ((Get-CoJVrVideoStreamHash $Backup) -ne $OriginalHash) {
            throw 'The VR video-profile backup hash does not match the recorded original settings.'
        }
        # Never create/truncate a missing destination until its backup verifies.
        # A concurrently created file makes CreateNew fail without touching it.
        if ($Missing) { $Destination = [IO.File]::Open($VideoPath, 'CreateNew', 'ReadWrite', 'None') }
        if (-not $Missing -and $CurrentHash -ne [string]$State.appliedSha256 -and
            -not (@($State.conflicts | Where-Object { $_.sha256 -eq $CurrentHash }).Count)) {
            $ConflictPath = $VideoPath + '.conflict-' + [guid]::NewGuid().ToString('N')
            # CreateNew semantics: no existing file can be overwritten. A failed
            # or interrupted copy stays on disk; the original journal/backup stay
            # retryable. Cleanup never deletes conflict copies, journaled or not.
            $ConflictStream = [IO.File]::Open($ConflictPath, 'CreateNew', 'Write', 'None')
            try {
                $Destination.Position = 0
                $Destination.CopyTo($ConflictStream)
                $ConflictStream.Flush($true)
            } finally { $ConflictStream.Dispose() }
            if ((Get-FileHash -LiteralPath $ConflictPath -Algorithm SHA256 -ErrorAction Stop).Hash -ne $CurrentHash) {
                throw 'The VR video-profile conflict copy failed verification; recovery evidence retained.'
            }
            $Conflicts = @($State.conflicts | Where-Object { $null -ne $_ }) + @([pscustomobject]@{
                path = $ConflictPath
                sha256 = $CurrentHash
            })
            $State | Add-Member -NotePropertyName conflicts -NotePropertyValue $Conflicts -Force
            Write-CoJVrVideoRecoveryJournal -StatePath $StatePath -State $State
        }
        $Backup.Position = 0
        $Destination.Position = 0
        $Backup.CopyTo($Destination)
        $Destination.SetLength($Backup.Length)
        $Destination.Flush($true)
    }
    if ((Get-CoJVrVideoStreamHash $Destination) -ne $OriginalHash) {
        throw 'Video.scr does not match the recorded restored settings; recovery evidence retained.'
    }
    # Commit restoration before cleanup. Either deletion can fail independently;
    # this journal lets finish retry without rewriting already-restored settings.
    $State | Add-Member -NotePropertyName restoreComplete -NotePropertyValue $true -Force
    Write-CoJVrVideoRecoveryJournal -StatePath $StatePath -State $State
    } finally {
        if ($null -ne $Backup) { $Backup.Dispose() }
        if ($null -ne $Destination) { $Destination.Dispose() }
    }
    if (Test-Path -LiteralPath $BackupPath -PathType Leaf) { Remove-Item -LiteralPath $BackupPath -Force }
    Remove-Item -LiteralPath $StatePath -Force
    Write-Host 'Restored the original Call of Juarez Video.scr settings.'
    return $true
}

function Get-CoJVrVideoStreamHash {
    param([IO.Stream]$Stream)
    $Hasher = [Security.Cryptography.SHA256]::Create()
    try {
        $Stream.Position = 0
        return [BitConverter]::ToString($Hasher.ComputeHash($Stream)).Replace('-', '')
    } finally { $Hasher.Dispose() }
}

function Write-CoJVrVideoRecoveryJournal {
    param([string]$StatePath, [object]$State)
    # Replace atomically so a failed/interrupted journal update cannot truncate
    # the previous recovery record. The temporary file is adjacent for one volume.
    $TemporaryPath = $StatePath + '.tmp-' + [guid]::NewGuid().ToString('N')
    try {
        [IO.File]::WriteAllText($TemporaryPath, ($State | ConvertTo-Json -Depth 8) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
        [IO.File]::Replace($TemporaryPath, $StatePath, [NullString]::Value)
    } finally {
        if (Test-Path -LiteralPath $TemporaryPath -PathType Leaf) { Remove-Item -LiteralPath $TemporaryPath -Force }
    }
}
