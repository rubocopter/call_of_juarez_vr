function Assert-CoJExecutableProvenance($Run, $Stage) {
    $Original = '5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE'
    $Derived = 'C8B8BB82FCB3D6599C5F77B1BB9CB3444CBB3A360461DAD43AD808B49AD28DC9'
    $Hash = ([string]$Run.game.executable.sha256).ToUpperInvariant()
    $Assets = @($Run.deployment | Where-Object { [string]$_.role -eq 'game_executable' })
    if (-not [bool]$Run.game.executable.knownExactBuild) {
        throw 'Run executable is not an exact recognized CoJ image.'
    }
    if ($Hash -eq $Original) {
        if ([bool]$Run.game.executable.largeAddressAware -or [bool]$Stage.executableManaged -or $Assets.Count) {
            throw 'Original executable has contradictory staged LAA metadata.'
        }
        return
    }
    if ($Hash -ne $Derived -or -not [bool]$Run.game.executable.largeAddressAware -or
        [string]$Run.game.executable.originalSha256 -ne $Original -or
        -not [bool]$Stage.executableManaged -or
        [string]$Stage.originalExecutableSha256 -ne $Original -or
        [string]$Stage.stagedExecutableSha256 -ne $Derived -or
        $Assets.Count -ne 1 -or [string]$Assets[0].destination -ne 'CoJ.exe' -or
        [string]$Assets[0].sha256 -ne $Derived -or [string]$Assets[0].originalSha256 -ne $Original) {
        throw 'LAA executable lacks exact original/derived deployment and restoration provenance.'
    }
}
