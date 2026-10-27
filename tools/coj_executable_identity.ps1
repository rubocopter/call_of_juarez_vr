function Get-CoJExactExecutableBuilds {
    @(
        [pscustomobject]@{
            Edition = 'steam'
            Original = '5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE'
            Derived = 'C8B8BB82FCB3D6599C5F77B1BB9CB3444CBB3A360461DAD43AD808B49AD28DC9'
            Data0 = '5EB6EC4856E53226DFABAD929522706A21F02D428923A8C9C38804CC2CF11A94'
        },
        [pscustomobject]@{
            Edition = 'gog'
            Original = '8BAB3D109BD4C08BF356D30DE1B746CDC3A89DEBF4C953763B1305FE0C8285BD'
            Derived = 'E9D06B14EB70BBC57135C1C63000E5EC7E16C277B8115184CABF29F733729C19'
            Data0 = 'F516250B98F73C872CA5A8CD0FDDD2C1EB74C8F118B9C99EFFE9C581C5382DD3'
        }
    )
}

function Get-CoJExactExecutableBuild([string]$OriginalHash) {
    foreach ($Build in (Get-CoJExactExecutableBuilds)) {
        if ($Build.Original -eq $OriginalHash.ToUpperInvariant()) { return $Build }
    }
    return $null
}

function Assert-CoJExecutableProvenance($Run, $Stage) {
    $Hash = ([string]$Run.game.executable.sha256).ToUpperInvariant()
    $Assets = @($Run.deployment | Where-Object { [string]$_.role -eq 'game_executable' })
    if (-not [bool]$Run.game.executable.knownExactBuild) {
        throw 'Run executable is not an exact recognized CoJ image.'
    }
    foreach ($Build in (Get-CoJExactExecutableBuilds)) {
        if ($Hash -eq $Build.Original) {
            if ([bool]$Run.game.executable.largeAddressAware -or [bool]$Stage.executableManaged -or $Assets.Count) {
                throw 'Original executable has contradictory staged LAA metadata.'
            }
            return
        }
    }
    $Build = Get-CoJExactExecutableBuild ([string]$Run.game.executable.originalSha256)
    if ($null -eq $Build) { throw 'Unknown executable source image.' }
    $Original = $Build.Original
    $Derived = $Build.Derived
    if ($Hash -ne $Derived -or -not [bool]$Run.game.executable.largeAddressAware -or
        -not [bool]$Stage.executableManaged -or
        [string]$Stage.originalExecutableSha256 -ne $Original -or
        [string]$Stage.stagedExecutableSha256 -ne $Derived -or
        $Assets.Count -ne 1 -or [string]$Assets[0].destination -ne 'CoJ.exe' -or
        [string]$Assets[0].sha256 -ne $Derived -or [string]$Assets[0].originalSha256 -ne $Original) {
        throw 'LAA executable lacks exact original/derived deployment and restoration provenance.'
    }
}
