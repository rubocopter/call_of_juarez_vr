# Shared public installer policy; importing this file has no side effects.
# Native children of PowerShell 7 (including CTest/cmd.exe) can inherit its
# module path. Load the host's own built-in utility module, without depending
# on profiles or changing the user's module search path.
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility') -ErrorAction Stop
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Security') -ErrorAction Stop
function Write-CojvrPublicJson([string]$Path, [object]$Value) {
    $Temp = $Path + '.writing-' + [guid]::NewGuid().ToString('N')
    try {
        [IO.File]::WriteAllText($Temp, ($Value | ConvertTo-Json -Depth 20), [Text.UTF8Encoding]::new($false))
        Move-Item -LiteralPath $Temp -Destination $Path -Force
    } finally { if (Test-Path -LiteralPath $Temp) { Remove-Item -LiteralPath $Temp -Force } }
}
function Get-CojvrPathMetadata([string]$Path) {
    # FindFirstFile reports the tag without opening/hydrating a cloud placeholder
    # or following a leaf link. PowerShell 5.1 FileInfo exposes only attributes.
    if (-not ('Cojvr.ReleasePathMetadata' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
namespace Cojvr {
    public static class ReleasePathMetadata {
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        public struct Info {
            public uint Attributes;
            public System.Runtime.InteropServices.ComTypes.FILETIME CreationTime, AccessTime, WriteTime;
            public uint SizeHigh, SizeLow, ReparseTag, Reserved;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)] public string Name;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 14)] public string AlternateName;
        }
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        static extern IntPtr FindFirstFileW(string path, out Info info);
        [DllImport("kernel32.dll")] static extern bool FindClose(IntPtr handle);
        public static object Read(string path) {
            Info info;
            IntPtr handle = FindFirstFileW(path, out info);
            if (handle == new IntPtr(-1)) {
                int error = Marshal.GetLastWin32Error();
                if (error == 2 || error == 3) return null;
                throw new Win32Exception(error, "Cannot inspect deployment path: " + path);
            }
            try { return info; } finally { FindClose(handle); }
        }
    }
}
'@
    }
    if ($Path -eq [IO.Path]::GetPathRoot($Path)) {
        return [pscustomobject]@{Attributes=[IO.File]::GetAttributes($Path);ReparseTag=0}
    }
    return [Cojvr.ReleasePathMetadata]::Read($Path)
}
function Assert-CojvrNoReparse([string]$Path, [switch]$AllowCloudFiles) {
    if ([string]::IsNullOrWhiteSpace($Path)) { throw 'A deployment path is required.' }
    $Probe = [IO.Path]::GetFullPath($Path)
    $Root = [IO.Path]::GetPathRoot($Probe)
    if ($Probe.Substring($Root.Length).Contains(':') -or $Probe.IndexOfAny([char[]]'*?') -ge 0) {
        throw 'Deployment paths must be literal files/directories, without streams or wildcards.'
    }
    $Probe = $Probe.TrimEnd('\')
    if ($Probe.Length -lt $Root.Length) { $Probe = $Root }
    # Missing destinations still require checking all their existing ancestors.
    while ($null -ne $Probe) {
        $Info = Get-CojvrPathMetadata $Probe
        if ($null -ne $Info -and ($Info.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            # Only the documented Cloud Files family (0x9000001A..0x9000F01A).
            # These store provider data, not another named destination. Unknown
            # tags, name-surrogate tags, junctions and symlinks always fail closed.
            if (-not $AllowCloudFiles -or ([uint32]$Info.ReparseTag -band [uint32]4294905855) -ne [uint32]2415919130) {
                throw "Linked paths are not admitted for deployment: '$Path' (component '$Probe', tag $('0x{0:X8}' -f [uint32]$Info.ReparseTag))."
            }
        }
        $Probe = [IO.Path]::GetDirectoryName($Probe)
    }
}
function Get-CojvrResolutionSettingsHelper([string]$LauncherDirectory) {
    $Directory = [IO.Path]::GetFullPath($LauncherDirectory)
    $Packaged = Join-Path $Directory 'tools/coj_resolution_settings.ps1'
    if (Test-Path -LiteralPath (Join-Path $Directory 'release-manifest.json') -PathType Leaf) {
        if (-not (Test-Path -LiteralPath $Packaged -PathType Leaf)) {
            throw 'Missing resolution helper. Extract the complete release ZIP again.'
        }
        return $Packaged
    }
    # Resolution preferences may be edited from an actual developer checkout.
    # This exception only exposes a launcher preference file, never deployment.
    $Repository = Split-Path -Parent $Directory
    $SourceHelper = Join-Path $Repository 'tools/coj_resolution_settings.ps1'
    if ([IO.Path]::GetFileName($Directory) -cne 'release' -or
        -not (Test-Path -LiteralPath (Join-Path $Repository 'AGENTS.md') -PathType Leaf) -or
        -not (Test-Path -LiteralPath (Join-Path $Repository 'docs/ARCHITECTURE.md') -PathType Leaf) -or
        -not (Test-Path -LiteralPath $SourceHelper -PathType Leaf)) {
        throw 'Complete Call of Juarez VR release package required. Extract the release ZIP before using this action.'
    }
    return $SourceHelper
}
function Get-CojvrGuiActionScript([string]$LauncherDirectory, [string]$Action) {
    $Directory = [IO.Path]::GetFullPath($LauncherDirectory)
    if ($Action -cnotin @('Verify','Install','Uninstall','Diagnose','Status','PrepareSession','ConfigureResolution')) {
        throw 'Unsupported graphical launcher action.'
    }
    if (Test-Path -LiteralPath (Join-Path $Directory 'release-manifest.json') -PathType Leaf) {
        return (Join-Path $Directory 'cojvr.ps1')
    }
    # Admit only an actual checkout. Incomplete player ZIPs must never fall back
    # to unverified payloads or an arbitrary package found elsewhere on disk.
    [void](Get-CojvrResolutionSettingsHelper $Directory)
    if ($Action -eq 'ConfigureResolution') { return (Join-Path $Directory 'cojvr.ps1') }
    $Backend = Join-Path (Split-Path -Parent $Directory) 'tools/cojvr_source_action.ps1'
    if (-not (Test-Path -LiteralPath $Backend -PathType Leaf)) { throw 'Missing local installer package builder.' }
    return $Backend
}
function Read-CojvrRelease([string]$PackageRoot) {
    $Root = [IO.Path]::GetFullPath($PackageRoot).TrimEnd('\') + '\'
    Assert-CojvrNoReparse $Root
    $Manifest = Get-Content -LiteralPath (Join-Path $Root 'release-manifest.json') -Raw | ConvertFrom-Json
    if ($Manifest.schemaVersion -ne 1 -or $Manifest.manifestType -ne 'cojvr-release' -or
        $Manifest.version -notmatch '^\d+\.\d+\.\d+-prealpha\.\d+$' -or
        $Manifest.sourceCommit -notmatch '^[a-fA-F0-9]{40}$' -or @($Manifest.files).Count -lt 1) {
        throw 'Unsupported release manifest.'
    }
    $Seen = @{}
    foreach ($Entry in $Manifest.files) {
        $Relative = [string]$Entry.path
        if ($Relative -notmatch '^[A-Za-z0-9_.-]+(/[A-Za-z0-9_.-]+)*$' -or
            @($Relative.Split('/') | Where-Object { $_ -eq '.' -or $_ -eq '..' }).Count -ne 0 -or
            $Relative -eq 'release-manifest.json' -or $Seen.ContainsKey($Relative)) {
            throw "Invalid or duplicated release path '$Relative'."
        }
        $Seen[$Relative] = $true
        $File = Join-Path $Root $Relative
        if (-not (Test-Path -LiteralPath $File -PathType Leaf)) { throw "Missing packaged file '$Relative'. Extract the complete ZIP again." }
        Assert-CojvrNoReparse $File
        if ([string]$Entry.sha256 -notmatch '^[a-fA-F0-9]{64}$' -or
            (Get-Item -LiteralPath $File).Length -ne [long]$Entry.size -or
            (Get-FileHash -LiteralPath $File -Algorithm SHA256).Hash -ne $Entry.sha256) {
            throw "Packaged file '$Relative' failed verification. Download/extract the release again."
        }
    }
    foreach ($Required in @('cojvr.ps1','cojvr-gui.ps1','release_common.ps1','CoJ-VR.cmd',
        'payload/d3d9_native_stereo.dll','payload/openvr_api.dll',
        'payload/d3d9_native_stereo.build-manifest.json','payload/cojvr_openvr_input/actions.json',
        'dependencies/python/python.exe','dependencies/vc_redist.x86.exe',
        'tools/stage_d3d9_proxy.ps1','tools/unstage_d3d9_proxy.ps1',
        'tools/coj_executable_identity.ps1','tools/deployment_transaction.ps1','tools/coj_video_profile.ps1','tools/set_hmd_camera_control.ps1',
        'tools/coj_resolution_settings.ps1','tools/coj_resolution_session.ps1','tools/coj_resolution_profiles.py',
        'tools/cojvr_openvr_resolution_query.exe','tools/cojvr_d3d9_resolution_limits.exe','tools/openvr_api.dll')) {
        if (-not $Seen.ContainsKey($Required)) { throw "Release inventory omits required file '$Required'." }
    }
    foreach ($Item in Get-ChildItem -LiteralPath $Root -Recurse -Force) {
        if (($Item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Release contains linked content.' }
        if (-not $Item.PSIsContainer) {
            $Relative = $Item.FullName.Substring($Root.Length).Replace('\','/')
            if ($Relative -ne 'release-manifest.json' -and -not $Seen.ContainsKey($Relative)) {
                throw "Uninventoried release file '$Relative'. Extract into a new empty folder."
            }
        }
    }
    return $Manifest
}
function Assert-CojvrPublicOwnership([string]$GameDirectory) {
    Assert-CojvrNoReparse $GameDirectory
    foreach ($Name in @('.cojvr-d3d9-stage.json','.cojvr-deployment-transaction.json')) {
        if ((Test-Path -LiteralPath (Join-Path $GameDirectory $Name)) -and
            -not (Test-Path -LiteralPath (Join-Path $GameDirectory '.cojvr-public-install.json'))) {
            throw 'A developer candidate or foreign deployment is active. Restore it with its original workflow first.'
        }
    }
    $Path = Join-Path $GameDirectory '.cojvr-public-install.json'
    if (Test-Path -LiteralPath $Path) {
        Assert-CojvrNoReparse $Path
        $State = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
        if ($State.schemaVersion -ne 1 -or $State.manifestType -ne 'cojvr-public-install' -or
            $State.installId -notmatch '^[a-f0-9]{32}$' -or $State.buildManifestId -notmatch '^[A-Fa-f0-9]{64}$' -or
            $State.sourceCommit -notmatch '^[A-Fa-f0-9]{40}$' -or $State.phase -notin @('installing','installed') -or
            $State.version -notmatch '^\d+\.\d+\.\d+-prealpha\.\d+$' -or
            $State.runId -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$' -or
            ($State.PSObject.Properties['pendingRunId'] -and $State.pendingRunId -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$')) { throw 'Invalid public installation recovery state. Preserve backups.' }
        return $State
    }
    return $null
}
function Assert-CojvrPublicGame([string]$GameDirectory, [switch]$Installed) {
    # One exact-build catalog for developer and player workflows.
    $IdentityHelper = Join-Path $PSScriptRoot 'tools/coj_executable_identity.ps1'
    if (-not (Test-Path -LiteralPath $IdentityHelper)) {
        $IdentityHelper = Join-Path (Split-Path $PSScriptRoot -Parent) 'tools/coj_executable_identity.ps1'
    }
    . $IdentityHelper
    Assert-CojvrNoReparse $GameDirectory
    $Executable = Join-Path $GameDirectory 'CoJ.exe'
    if ($Installed -and (Test-Path -LiteralPath (Join-Path $GameDirectory 'CoJ.cojvr-backup.exe'))) {
        $Executable = Join-Path $GameDirectory 'CoJ.cojvr-backup.exe'
    }
    Assert-CojvrNoReparse $Executable
    $Build = Get-CoJExactExecutableBuild (Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash
    if ($null -eq $Build) { throw 'Unsupported or modified CoJ.exe. Only the verified Steam and experimental GOG (2006) builds are admitted.' }
    $Expected = @{
        'CoJ.exe'=$Build.Original
        'ChromeEngine3.dll'='DB69BC35919FE57187766771A2452ACA11090474F6D63DF1A85A80EDED131EC8'
        'code.pak'='F9DB47C166E03F23E37CBCDFD5344E4AD4C5C9134F35E8F6DCDF66DB7E71CE12'
        'Data0.pak'=$Build.Data0
    }
    Assert-CojvrNoReparse $GameDirectory
    foreach ($Name in $Expected.Keys) {
        $Path = Join-Path $GameDirectory $Name
        if ($Installed -and $Name -ne 'ChromeEngine3.dll') {
            $BackupName = @{ 'CoJ.exe'='CoJ.cojvr-backup.exe';'code.pak'='code.cojvr-backup.pak';'Data0.pak'='Data0.cojvr-backup.pak' }[$Name]
            if (Test-Path -LiteralPath (Join-Path $GameDirectory $BackupName)) { $Path = Join-Path $GameDirectory $BackupName }
        }
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Required game file '$Name' is missing. Run the original D3D9 game once first." }
        Assert-CojvrNoReparse $Path
        if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Expected[$Name]) {
            throw "Unsupported or modified '$Name'. This pre-alpha requires matching original files from the verified Steam or experimental GOG CoJ (2006) build. No files were patched."
        }
    }
    foreach ($Item in Get-ChildItem -LiteralPath $GameDirectory -Force) {
        if ($Item.Name -like '*cojvr*' -or $Item.Name -in @('d3d9.dll','openvr_api.dll')) { Assert-CojvrNoReparse $Item.FullName }
    }
    return $Build
}
function Get-CojvrPublicGameCandidates([string[]]$SteamRoots = @(), [string[]]$GogRoots = @()) {
    # Explicit fixture roots keep discovery deterministic in host tests.
    if (-not $PSBoundParameters.ContainsKey('GogRoots') -and $SteamRoots.Count -eq 0) {
        foreach ($Key in @('HKLM:\SOFTWARE\WOW6432Node\GOG.com\Games\1909524379',
                            'HKLM:\SOFTWARE\GOG.com\Games\1909524379',
                            'HKCU:\Software\GOG.com\Games\1909524379')) {
            $Entry = Get-ItemProperty -LiteralPath $Key -ErrorAction SilentlyContinue
            if ($null -ne $Entry -and $Entry.PSObject.Properties['path'] -and $Entry.path) { $GogRoots += $Entry.path }
        }
        foreach ($Base in @($env:ProgramFiles,${env:ProgramFiles(x86)},'C:\GOG Games')) {
            if ($Base) {
                $GogRoots += (Join-Path $Base 'GOG Galaxy/Games/Call of Juarez')
                $GogRoots += (Join-Path $Base 'Call of Juarez')
            }
        }
    }
    # Steam AppID 3020 is Call of Juarez (2006). Read its appmanifest because
    # Steam lets users rename the installation folder or move libraries.
    if ($SteamRoots.Count -eq 0) {
        foreach ($Key in @('HKCU:\Software\Valve\Steam','HKLM:\SOFTWARE\Valve\Steam','HKLM:\SOFTWARE\WOW6432Node\Valve\Steam')) {
            $Steam = Get-ItemProperty -LiteralPath $Key -ErrorAction SilentlyContinue
            if ($null -ne $Steam) { $SteamRoots += @($Steam.SteamPath, $Steam.InstallPath) | Where-Object { $_ } }
        }
        if (${env:ProgramFiles(x86)}) { $SteamRoots += (Join-Path ${env:ProgramFiles(x86)} 'Steam') }
        if ($env:ProgramFiles) { $SteamRoots += (Join-Path $env:ProgramFiles 'Steam') }
    }
    $Libraries = @()
    foreach ($SteamRoot in $SteamRoots) {
        if ([string]::IsNullOrWhiteSpace($SteamRoot) -or -not (Test-Path -LiteralPath $SteamRoot -PathType Container)) { continue }
        $Libraries += $SteamRoot
        $Vdf = Join-Path $SteamRoot 'steamapps/libraryfolders.vdf'
        if (Test-Path -LiteralPath $Vdf -PathType Leaf) {
            try {
                foreach ($Match in [regex]::Matches([IO.File]::ReadAllText($Vdf), '"path"\s*"([^"]+)"')) {
                    $Libraries += $Match.Groups[1].Value.Replace('\\','\')
                }
            } catch [IO.IOException] { } # Fall back to primary Steam library.
        }
    }
    $Seen = @{}
    foreach ($Library in $Libraries | Select-Object -Unique) {
        if (-not (Test-Path -LiteralPath $Library -PathType Container)) { continue }
        $SteamApps = Join-Path $Library 'steamapps'
        $Dirs = @('Call of Juarez') # Legacy installs without a Steam appmanifest.
        $Manifest = Join-Path $SteamApps 'appmanifest_3020.acf'
        if (Test-Path -LiteralPath $Manifest -PathType Leaf) {
            try {
                $Match = [regex]::Match([IO.File]::ReadAllText($Manifest), '"installdir"\s*"([^"]+)"')
                if ($Match.Success) {
                    $Name = $Match.Groups[1].Value
                    if ($Name -and $Name -notin @('.','..') -and $Name -notmatch '[\\/:]' ) { $Dirs = @($Name) + $Dirs }
                }
            } catch [IO.IOException] { }
        }
        foreach ($Dir in $Dirs) {
            $Candidate = Join-Path $SteamApps (Join-Path 'common' $Dir)
            if ((Test-Path -LiteralPath (Join-Path $Candidate 'CoJ.exe') -PathType Leaf) -and
                (Test-Path -LiteralPath (Join-Path $Candidate 'ChromeEngine3.dll') -PathType Leaf)) {
                $Path = [IO.Path]::GetFullPath($Candidate)
                $Seen[$Path] = $Path
            }
        }
    }
    foreach ($Candidate in $GogRoots) {
        if ([string]::IsNullOrWhiteSpace($Candidate)) { continue }
        if ((Test-Path -LiteralPath (Join-Path $Candidate 'CoJ.exe') -PathType Leaf) -and
            (Test-Path -LiteralPath (Join-Path $Candidate 'ChromeEngine3.dll') -PathType Leaf)) {
            $Path = [IO.Path]::GetFullPath($Candidate).TrimEnd('\')
            $Seen[$Path] = $Path
        }
    }
    @($Seen.Values | Sort-Object)
}
function Resolve-CojvrPublicGame([string]$GameDirectory,[string[]]$SteamRoots = @(),[string[]]$GogRoots = @()) {
    if (-not [string]::IsNullOrWhiteSpace($GameDirectory)) {
        $Path = [IO.Path]::GetFullPath($GameDirectory.Trim().Trim('"'))
        if (-not (Test-Path -LiteralPath $Path -PathType Container)) { throw "Game directory does not exist: '$Path'." }
        return $Path
    }
    $Search = @{SteamRoots=$SteamRoots}
    if ($PSBoundParameters.ContainsKey('GogRoots')) { $Search.GogRoots=$GogRoots }
    $Candidates = @(Get-CojvrPublicGameCandidates @Search)
    if ($Candidates.Count -eq 1) { return $Candidates[0] }
    if ($Candidates.Count -gt 1) { throw 'Multiple Call of Juarez installs found. Select the intended folder explicitly.' }
    throw 'Call of Juarez (2006) was not found in Steam or GOG installations. Select the folder containing CoJ.exe.'
}
function New-CojvrPublicReport {
    param([string]$GameDirectory, [string]$OutputDirectory, [string]$Version)
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    $Name = 'cojvr-report-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8)
    $Folder = Join-Path $OutputDirectory $Name
    New-Item -ItemType Directory -Path $Folder | Out-Null
    $Names = @('cojvr.log','callstack.txt','.cojvr-public-install.json','.cojvr-run.json','.cojvr-d3d9-stage.json','cojvr-native-stereo-summary.json')
    $Run = $null
    try { $Run = Get-Content -LiteralPath (Join-Path $GameDirectory '.cojvr-run.json') -Raw | ConvertFrom-Json } catch { }
    if ($null -ne $Run -and $Run.runId -and $Run.buildManifestId) {
        foreach ($Fault in @(Get-ChildItem -LiteralPath $GameDirectory -Filter 'cojvr-render-fault-*.txt' -File | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 4)) {
            if ($Fault.Name -cnotmatch '^cojvr-render-fault-[0-9]+\.txt$' -or $Fault.Length -gt 16384) { continue }
            Assert-CojvrNoReparse $Fault.FullName
            $Text = [IO.File]::ReadAllText($Fault.FullName)
            if ($Text -cmatch ('(?m)^run_id=' + [regex]::Escape([string]$Run.runId) + '\r?$') -and
                $Text -cmatch ('(?m)^build_manifest_id=' + [regex]::Escape([string]$Run.buildManifestId) + '\r?$')) { $Names += $Fault.Name }
        }
    }
    foreach ($Name in $Names) {
        $Path = Join-Path $GameDirectory $Name
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { continue }
        Assert-CojvrNoReparse $Path
        # Bound memory and report size; never collect dumps, saves or archives.
        $Stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
        try {
            if ($Stream.Length -gt 4MB) { [void]$Stream.Seek(-4MB, [IO.SeekOrigin]::End) }
            $Reader = [IO.StreamReader]::new($Stream)
            try { $Text = $Reader.ReadToEnd() } finally { $Reader.Dispose() }
        } finally { $Stream.Dispose() }
        foreach ($PrivatePath in @($GameDirectory, $env:USERPROFILE, $env:LOCALAPPDATA, $env:TEMP)) {
            if (-not [string]::IsNullOrWhiteSpace($PrivatePath)) {
                $Text = [regex]::Replace($Text, [regex]::Escape($PrivatePath.Replace('\','\\')), '<local-path>', [Text.RegularExpressions.RegexOptions]::IgnoreCase)
                $Text = [regex]::Replace($Text, [regex]::Escape($PrivatePath), '<local-path>', [Text.RegularExpressions.RegexOptions]::IgnoreCase)
            }
        }
        [IO.File]::WriteAllText((Join-Path $Folder $Name.TrimStart('.')), $Text, [Text.UTF8Encoding]::new($false))
    }
    $Info = @{schemaVersion=1;releaseVersion=$Version;createdUtc=[DateTime]::UtcNow.ToString('o');osVersion=[Environment]::OSVersion.Version.ToString();is64BitOperatingSystem=[Environment]::Is64BitOperatingSystem;headsetAcceptance='not_evaluated';note='Community report, not canonical validation. Inspect text for private information before attaching manually.'}
    Write-CojvrPublicJson (Join-Path $Folder 'report.json') $Info
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $Zip = $Folder + '.zip'
    [IO.Compression.ZipFile]::CreateFromDirectory($Folder, $Zip)
    return $Zip
}
