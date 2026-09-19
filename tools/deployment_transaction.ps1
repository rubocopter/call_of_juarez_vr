$ErrorActionPreference = "Stop"

function Invoke-CojvrDeploymentCheckpoint([string]$Name) {
    if ([string]$env:COJVR_DEPLOYMENT_FAILURE_TEST -ne "1") { return }
    $Requested = [string]$env:COJVR_DEPLOYMENT_FAIL_AFTER
    if (-not [string]::IsNullOrWhiteSpace($Requested) -and $Requested -eq $Name) {
        throw "Injected deployment failure after checkpoint '$Name'."
    }
}

function Get-CojvrFileSha256([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Get-CojvrDirectoryManifest([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) { return @() }
    $Root = [System.IO.Path]::GetFullPath($Path).TrimEnd("\")
    return @(
        Get-ChildItem -LiteralPath $Root -File -Recurse | Sort-Object FullName | ForEach-Object {
            [ordered]@{
                path = $_.FullName.Substring($Root.Length).TrimStart("\").Replace("\", "/")
                sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
            }
        }
    )
}

function Test-CojvrDirectoryMatchesManifest([string]$Path, [object[]]$Manifest) {
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) { return $false }
    $Actual = @(Get-CojvrDirectoryManifest $Path)
    $Expected = @($Manifest)
    if ($Actual.Count -ne $Expected.Count) { return $false }
    for ($Index = 0; $Index -lt $Expected.Count; ++$Index) {
        if ([string]$Actual[$Index].path -ne [string]$Expected[$Index].path -or
            ([string]$Actual[$Index].sha256).ToUpperInvariant() -ne
                ([string]$Expected[$Index].sha256).ToUpperInvariant()) {
            return $false
        }
    }
    return $true
}

function Test-CojvrDirectoryIsKnownStagedSubset([string]$Path, [object[]]$Manifest) {
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) { return $true }
    $Expected = @{}
    foreach ($Entry in @($Manifest)) {
        $Expected[[string]$Entry.path] = ([string]$Entry.sha256).ToUpperInvariant()
    }
    foreach ($Actual in @(Get-CojvrDirectoryManifest $Path)) {
        $Relative = [string]$Actual.path
        if (-not $Expected.ContainsKey($Relative) -or
            ([string]$Actual.sha256).ToUpperInvariant() -ne $Expected[$Relative]) {
            return $false
        }
    }
    return $true
}

function Install-CojvrVerifiedFile(
    [string]$Source,
    [string]$Destination,
    [string]$Temporary,
    [string]$ExpectedSha256,
    [string]$Checkpoint = "file_published") {
    if (Test-Path -LiteralPath $Temporary) {
        throw "Temporary deployment path '$Temporary' already exists."
    }
    Copy-Item -LiteralPath $Source -Destination $Temporary
    $Expected = $ExpectedSha256.ToUpperInvariant()
    if ((Get-CojvrFileSha256 $Temporary) -ne $Expected) {
        Remove-Item -LiteralPath $Temporary -Force
        throw "Temporary deployment copy '$Temporary' failed SHA-256 verification."
    }
    Move-Item -LiteralPath $Temporary -Destination $Destination
    Invoke-CojvrDeploymentCheckpoint $Checkpoint
    if ((Get-CojvrFileSha256 $Destination) -ne $Expected) {
        throw "Installed deployment file '$Destination' failed SHA-256 verification."
    }
}

function Install-CojvrVerifiedBytes(
    [byte[]]$Bytes,
    [string]$Destination,
    [string]$Temporary,
    [string]$ExpectedSha256,
    [string]$Checkpoint = "bytes_published") {
    if (Test-Path -LiteralPath $Temporary) {
        throw "Temporary deployment path '$Temporary' already exists."
    }
    [System.IO.File]::WriteAllBytes($Temporary, $Bytes)
    $Expected = $ExpectedSha256.ToUpperInvariant()
    if ((Get-CojvrFileSha256 $Temporary) -ne $Expected) {
        Remove-Item -LiteralPath $Temporary -Force
        throw "Temporary deployment content '$Temporary' failed SHA-256 verification."
    }
    Move-Item -LiteralPath $Temporary -Destination $Destination
    Invoke-CojvrDeploymentCheckpoint $Checkpoint
    if ((Get-CojvrFileSha256 $Destination) -ne $Expected) {
        throw "Installed deployment file '$Destination' failed SHA-256 verification."
    }
}

function Write-CojvrDeploymentJournal(
    [string]$GameDirectory,
    [string]$Operation,
    [string]$RunId,
    [string]$DiagnosticMode,
    [object[]]$Assets) {
    $JournalPath = Join-Path $GameDirectory ".cojvr-deployment-transaction.json"
    if (Test-Path -LiteralPath $JournalPath) {
        throw "Deployment transaction journal already exists at '$JournalPath'."
    }
    $Journal = [ordered]@{
        schemaVersion = 1
        manifestType = "cojvr-deployment-transaction"
        operation = $Operation
        state = "prepared"
        createdUtc = [DateTime]::UtcNow.ToString("o")
        runId = $RunId
        diagnosticMode = $DiagnosticMode
        assets = @($Assets)
    }
    [System.IO.File]::WriteAllText(
        $JournalPath,
        ($Journal | ConvertTo-Json -Depth 12) + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))
    Invoke-CojvrDeploymentCheckpoint "journal_written"
    return $JournalPath
}

function Complete-CojvrDeploymentRecovery([string]$GameDirectory) {
    $GameDirectory = [System.IO.Path]::GetFullPath($GameDirectory)
    $JournalPath = Join-Path $GameDirectory ".cojvr-deployment-transaction.json"
    if (-not (Test-Path -LiteralPath $JournalPath -PathType Leaf)) { return $false }

    $Journal = Get-Content -LiteralPath $JournalPath -Raw | ConvertFrom-Json
    if ([int]$Journal.schemaVersion -ne 1 -or
        [string]$Journal.manifestType -ne "cojvr-deployment-transaction" -or
        [string]$Journal.state -ne "prepared") {
        throw "Unsupported or ambiguous deployment transaction journal '$JournalPath'."
    }

    foreach ($Asset in @($Journal.assets)) {
        $Destination = Join-Path $GameDirectory ([string]$Asset.destination)
        $Backup = if ([string]::IsNullOrWhiteSpace([string]$Asset.backup)) {
            $null
        } else {
            Join-Path $GameDirectory ([string]$Asset.backup)
        }
        $Temporary = if ([string]::IsNullOrWhiteSpace([string]$Asset.temporary)) {
            $null
        } else {
            Join-Path $GameDirectory ([string]$Asset.temporary)
        }
        $HadOriginal = [bool]$Asset.hadOriginal
        $Kind = [string]$Asset.kind

        if ($Kind -eq "file") {
            $OriginalHash = ([string]$Asset.originalSha256).ToUpperInvariant()
            $StagedHash = ([string]$Asset.stagedSha256).ToUpperInvariant()
            if ($Temporary -and (Test-Path -LiteralPath $Temporary)) {
                if (-not (Test-Path -LiteralPath $Temporary -PathType Leaf) -or
                    (Get-CojvrFileSha256 $Temporary) -ne $StagedHash) {
                    throw "Recovery temporary file '$Temporary' changed externally; refusing to remove it."
                }
                Remove-Item -LiteralPath $Temporary -Force
            }
            $BackupExists = $Backup -and (Test-Path -LiteralPath $Backup -PathType Leaf)
            if ($BackupExists -and $HadOriginal) {
                if ((Get-CojvrFileSha256 $Backup) -ne $OriginalHash) {
                    throw "Recovery backup '$Backup' changed after the transaction started."
                }
            }

            if (Test-Path -LiteralPath $Destination -PathType Leaf) {
                $CurrentHash = Get-CojvrFileSha256 $Destination
                if ($BackupExists) {
                    if ($CurrentHash -eq $StagedHash) {
                        Remove-Item -LiteralPath $Destination -Force
                    } elseif ($CurrentHash -eq $OriginalHash) {
                        Remove-Item -LiteralPath $Backup -Force
                        $BackupExists = $false
                    } else {
                        throw "Recovery destination '$Destination' changed externally; refusing to overwrite it."
                    }
                } elseif ($HadOriginal) {
                    if ($CurrentHash -ne $OriginalHash) {
                        throw "Original '$Destination' cannot be proven after interrupted deployment."
                    }
                } elseif ($CurrentHash -eq $StagedHash) {
                    Remove-Item -LiteralPath $Destination -Force
                } else {
                    throw "Recovery destination '$Destination' changed externally; refusing to remove it."
                }
            }

            if ($HadOriginal) {
                if ($BackupExists) {
                    Move-Item -LiteralPath $Backup -Destination $Destination
                } elseif (-not (Test-Path -LiteralPath $Destination -PathType Leaf) -or
                    (Get-CojvrFileSha256 $Destination) -ne $OriginalHash) {
                    throw "Recovery cannot restore the original '$Destination'."
                }
            } elseif ($BackupExists) {
                throw "Unexpected backup '$Backup' exists for an asset that had no original."
            }
            continue
        }

        if ($Kind -eq "directory") {
            $OriginalManifest = @($Asset.originalManifest)
            $StagedManifest = @($Asset.stagedManifest)
            if ($Temporary -and (Test-Path -LiteralPath $Temporary)) {
                if (-not (Test-Path -LiteralPath $Temporary -PathType Container) -or
                    -not (Test-CojvrDirectoryIsKnownStagedSubset $Temporary $StagedManifest)) {
                    throw "Recovery temporary directory '$Temporary' changed externally; refusing to remove it."
                }
                Remove-Item -LiteralPath $Temporary -Recurse -Force
            }
            $BackupExists = $Backup -and (Test-Path -LiteralPath $Backup -PathType Container)
            if ($BackupExists -and $HadOriginal -and
                -not (Test-CojvrDirectoryMatchesManifest $Backup $OriginalManifest)) {
                throw "Recovery directory backup '$Backup' changed after the transaction started."
            }

            if (Test-Path -LiteralPath $Destination -PathType Container) {
                if ($BackupExists) {
                    if (Test-CojvrDirectoryIsKnownStagedSubset $Destination $StagedManifest) {
                        Remove-Item -LiteralPath $Destination -Recurse -Force
                    } elseif (Test-CojvrDirectoryMatchesManifest $Destination $OriginalManifest) {
                        Remove-Item -LiteralPath $Backup -Recurse -Force
                        $BackupExists = $false
                    } else {
                        throw "Recovery directory '$Destination' changed externally; refusing to overwrite it."
                    }
                } elseif ($HadOriginal) {
                    if (-not (Test-CojvrDirectoryMatchesManifest $Destination $OriginalManifest)) {
                        throw "Original directory '$Destination' cannot be proven after interrupted deployment."
                    }
                } elseif (Test-CojvrDirectoryIsKnownStagedSubset $Destination $StagedManifest) {
                    Remove-Item -LiteralPath $Destination -Recurse -Force
                } else {
                    throw "Recovery directory '$Destination' changed externally; refusing to remove it."
                }
            }

            if ($HadOriginal) {
                if ($BackupExists) {
                    Move-Item -LiteralPath $Backup -Destination $Destination
                } elseif (-not (Test-Path -LiteralPath $Destination -PathType Container) -or
                    -not (Test-CojvrDirectoryMatchesManifest $Destination $OriginalManifest)) {
                    throw "Recovery cannot restore original directory '$Destination'."
                }
            } elseif ($BackupExists) {
                throw "Unexpected directory backup '$Backup' exists for an asset that had no original."
            }
            continue
        }

        throw "Unknown deployment transaction asset kind '$Kind'."
    }

    $StageState = Join-Path $GameDirectory ".cojvr-d3d9-stage.json"
    if ([string]$Journal.operation -eq "stage" -and (Test-Path -LiteralPath $StageState -PathType Leaf)) {
        Remove-Item -LiteralPath $StageState -Force
    }
    if ([string]$Journal.operation -eq "unstage" -and (Test-Path -LiteralPath $StageState -PathType Leaf)) {
        Remove-Item -LiteralPath $StageState -Force
    }
    Remove-Item -LiteralPath $JournalPath -Force
    return $true
}
