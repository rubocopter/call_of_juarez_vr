param([Parameter(Mandatory)] [string]$HostPath,
      [Parameter(Mandatory)] [string]$FixturePath)
$ErrorActionPreference = 'Stop'
$evidence = Join-Path ([IO.Path]::GetTempPath()) ('cojvr-shutdown-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $evidence | Out-Null
foreach ($mode in @('missing', 'hooked')) {
    $log = Join-Path $evidence ($mode + '.log')
    $process = Start-Process -FilePath $HostPath -WindowStyle Hidden -PassThru `
        -ArgumentList @("`"$FixturePath`"", "`"$log`"", $mode)
    if (-not $process.WaitForExit(10000)) {
        $process.Kill()
        throw "Isolated shutdown host timed out ($mode): $evidence"
    }
    if ($process.ExitCode -ne 0) { throw "Shutdown host failed ($mode): $($process.ExitCode)" }
    $text = Get-Content -LiteralPath $log -Raw
    if ($mode -eq 'missing') {
        if ($text -notmatch 'owner_cleaned=false' -or $text -match 'worker_cleanup') {
            throw 'DLL atexit baseline did not reproduce skipped owner cleanup'
        }
    } else {
        if ($text -match 'owner_cleaned=false|hook_restore_failed' -or
            $text -notmatch 'before_destroy\r?\nworker_cleanup\r?\nowner_cleaned=true\r?\nhook_restored\r?\noriginal_destroy\r?\ndll_atexit' -or
            ([regex]::Matches($text, 'worker_cleanup')).Count -ne 1) {
            throw "Pre-exit hook did not clean/restore/forward before DLL atexit: $text"
        }
    }
    Write-Output "PASS $mode : $log"
}
