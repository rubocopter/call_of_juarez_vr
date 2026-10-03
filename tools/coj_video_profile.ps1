# Exact CoJ Video.scr policy. Paths are supplied by the physical workflow;
# importing this helper has no filesystem or staging side effects.
function Apply-CoJVrVideoProfile {
    param(
        [Parameter(Mandatory = $true)] [string]$VideoPath,
        [Parameter(Mandatory = $true)] [string]$StatePath,
        [Parameter(Mandatory = $true)] [string]$BackupPath
    )

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

    # Keep the selected game render extent and all other quality choices. FSAA
    # 0 retains the demonstrated non-MSAA GPU transport profile. HMD recommended
    # eye size is not the native Chrome render-target allocation boundary.
    $Value = $Fsaa[0].Groups['value']
    $Updated = $Text.Substring(0, $Value.Index) + '0' + $Text.Substring($Value.Index + $Value.Length)
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
        fsaa = 0
    }
    try {
        # Persist recovery before mutation so an interrupted apply is restorable.
        [IO.File]::WriteAllText($StatePath, ($State | ConvertTo-Json) + [Environment]::NewLine, $Encoding)
        if ($Updated -ne $Text) { [IO.File]::WriteAllBytes($VideoPath, $AppliedBytes) }
        if ((Get-FileHash -LiteralPath $VideoPath -Algorithm SHA256).Hash -ne $AppliedHash) {
            throw 'Video.scr does not match the recorded applied profile.'
        }
        Write-Host "Applied reversible VR video profile: $($State.resolution) (selected resolution preserved), FSAA 0."
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
    $State = Get-Content -LiteralPath $StatePath -Raw | ConvertFrom-Json
    $VideoPath = [string]$State.videoPath
    if ([string]::IsNullOrWhiteSpace($VideoPath) -or
        [string]$State.originalSha256 -notmatch '^[0-9a-fA-F]{64}$') {
        throw 'The VR video-profile recovery journal is invalid.'
    }
    $OriginalHash = ([string]$State.originalSha256).ToUpperInvariant()
    if (Test-Path -LiteralPath $BackupPath -PathType Leaf) {
        if ((Get-FileHash -LiteralPath $BackupPath -Algorithm SHA256).Hash -ne $OriginalHash) {
            throw 'The VR video-profile backup hash does not match the recorded original settings.'
        }
    } elseif (-not $State.restoreComplete) {
        throw "The VR video-profile backup is missing; refusing to overwrite '$VideoPath'."
    }
    if (-not $State.restoreComplete) {
        Copy-Item -LiteralPath $BackupPath -Destination $VideoPath -Force
    }
    if ((Get-FileHash -LiteralPath $VideoPath -Algorithm SHA256).Hash -ne $OriginalHash) {
        throw 'Video.scr does not match the recorded restored settings; recovery evidence retained.'
    }
    # Commit restoration before cleanup. Either deletion can fail independently;
    # this journal lets finish retry without rewriting already-restored settings.
    $State | Add-Member -NotePropertyName restoreComplete -NotePropertyValue $true -Force
    [IO.File]::WriteAllText($StatePath, ($State | ConvertTo-Json) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
    if (Test-Path -LiteralPath $BackupPath -PathType Leaf) { Remove-Item -LiteralPath $BackupPath -Force }
    Remove-Item -LiteralPath $StatePath -Force
    Write-Host 'Restored the original Call of Juarez Video.scr settings.'
    return $true
}
