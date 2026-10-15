[CmdletBinding()]
param(
    [ValidateSet('Win64', 'Android', 'IOS', 'Mac', 'Linux', 'PS5', 'All')]
    [string[]]$Platforms = @('Win64'),

    [string]$ProjectRoot,
    [string]$CacheDirectory,
    [switch]$ForceInstall,
    [switch]$SkipProtoGeneration,
    [switch]$ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
if ([string]::IsNullOrWhiteSpace($CacheDirectory)) {
    $CacheDirectory = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'ProjectM\DependencyCache'
}

$ProjectRoot = [IO.Path]::GetFullPath($ProjectRoot)
$PluginRoot = Join-Path $ProjectRoot 'Plugins\TurboLink'
$ThirdPartyRoot = Join-Path $PluginRoot 'Source\ThirdParty'
$InstallMarkerPath = Join-Path $ThirdPartyRoot '.projectm-lock.json'
$LockPath = Join-Path $PSScriptRoot 'TurboLink\turbolink.lock.json'
$GenerateScript = Join-Path $PSScriptRoot 'GenerateProto.ps1'

if (-not (Test-Path (Join-Path $ProjectRoot 'ProjectM.uproject'))) {
    throw "ProjectM.uproject not found under ProjectRoot: $ProjectRoot"
}
if (-not (Test-Path (Join-Path $PluginRoot 'TurboLink.uplugin'))) {
    throw "Vendored TurboLink source is missing: $PluginRoot"
}
if (-not (Test-Path $LockPath)) {
    throw "TurboLink lock file is missing: $LockPath"
}

$Lock = Get-Content -Raw -Encoding UTF8 $LockPath | ConvertFrom-Json
$ArchiveName = [string]$Lock.libraries.asset
$ArchiveUrl = [string]$Lock.libraries.url
$ExpectedArchiveSize = [int64]$Lock.libraries.sizeBytes
$ExpectedArchiveHash = ([string]$Lock.libraries.sha256).ToUpperInvariant()
$ArchivePath = Join-Path ([IO.Path]::GetFullPath($CacheDirectory)) $ArchiveName

$PlatformDirectoryMap = @{
    Win64  = 'win64'
    Android = 'android'
    IOS    = 'ios'
    Mac    = 'mac'
    Linux  = 'linux'
    PS5    = 'ps5'
}

if ($Platforms -contains 'All') {
    $Platforms = @('Win64', 'Android', 'IOS', 'Mac', 'Linux', 'PS5')
}
$Platforms = @($Platforms | Sort-Object -Unique)
$LibraryDirectories = @($Platforms | ForEach-Object { $PlatformDirectoryMap[$_] })

function Assert-FileHash {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$ExpectedHash,
        [Parameter(Mandatory = $true)][string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Description is missing: $Path"
    }

    $ActualHash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($ActualHash -ne $ExpectedHash.ToUpperInvariant()) {
        throw "$Description hash mismatch. Expected $ExpectedHash, got $ActualHash. File: $Path"
    }
}

function Get-CommonSentinels {
    return @(
        'abseil\include\absl\base\config.h',
        'grpc\include\grpcpp\grpcpp.h',
        'protobuf\include\google\protobuf\message.h',
        're2\include\re2\re2.h',
        'grpc\bin\grpc_cpp_plugin.exe',
        'protobuf\bin\protoc.exe'
    )
}

function Get-PlatformSentinels {
    param([Parameter(Mandatory = $true)][string]$Platform)

    $Result = [Collections.Generic.List[string]]::new()
    switch ($Platform) {
        'Win64' {
            foreach ($Config in @('Release', 'RelWithDebInfo')) {
                $Result.Add("grpc\lib\win64\$Config\grpc++.lib")
                $Result.Add("protobuf\lib\win64\$Config\libprotobuf.lib")
                $Result.Add("abseil\lib\win64\$Config\absl_base.lib")
                $Result.Add("re2\lib\win64\$Config\re2.lib")
            }
        }
        'Android' {
            foreach ($Arch in @('arm64-v8a', 'x86_64')) {
                foreach ($Config in @('Debug', 'Release')) {
                    $Result.Add("grpc\lib\android\$Arch\$Config\libgrpc++.a")
                    $Result.Add("protobuf\lib\android\$Arch\$Config\libprotobuf.a")
                    $Result.Add("abseil\lib\android\$Arch\$Config\libabsl_base.a")
                    $Result.Add("re2\lib\android\$Arch\$Config\libre2.a")
                }
            }
        }
        { $_ -in @('Linux', 'PS5') } {
            $Directory = $PlatformDirectoryMap[$Platform]
            foreach ($Config in @('Debug', 'Release')) {
                $Result.Add("grpc\lib\$Directory\$Config\libgrpc++.a")
                $Result.Add("protobuf\lib\$Directory\$Config\libprotobuf.a")
                $Result.Add("abseil\lib\$Directory\$Config\libabsl_base.a")
                $Result.Add("re2\lib\$Directory\$Config\libre2.a")
            }
        }
        { $_ -in @('IOS', 'Mac') } {
            $Directory = $PlatformDirectoryMap[$Platform]
            $Result.Add("grpc\lib\$Directory\libgrpc++.a")
            $Result.Add("protobuf\lib\$Directory\libprotobuf.a")
            $Result.Add("abseil\lib\$Directory\libabsl_base.a")
            $Result.Add("re2\lib\$Directory\libre2.a")
        }
    }
    return $Result.ToArray()
}

function Get-InstallUnits {
    $Units = [Collections.Generic.List[string]]::new()
    foreach ($Unit in @('abseil\include', 'grpc\include', 'protobuf\include', 're2\include', 'grpc\bin', 'protobuf\bin')) {
        $Units.Add($Unit)
    }
    foreach ($Directory in $LibraryDirectories) {
        foreach ($Dependency in @('abseil', 'grpc', 'protobuf', 're2')) {
            $Units.Add("$Dependency\lib\$Directory")
        }
    }
    return @($Units | Sort-Object -Unique)
}

function Get-ValidatedInstallMarker {
    if (-not (Test-Path -LiteralPath $InstallMarkerPath -PathType Leaf)) {
        Write-Verbose "TurboLink install marker is missing: $InstallMarkerPath"
        return $null
    }

    try {
        $Marker = Get-Content -Raw -Encoding UTF8 $InstallMarkerPath | ConvertFrom-Json
    }
    catch {
        Write-Verbose "TurboLink install marker is invalid JSON: $($_.Exception.Message)"
        return $null
    }

    foreach ($Property in @('schemaVersion', 'archive', 'sha256', 'platforms', 'files')) {
        if ($null -eq $Marker.PSObject.Properties[$Property]) {
            Write-Verbose "TurboLink install marker is missing property: $Property"
            return $null
        }
    }
    if ([int]$Marker.schemaVersion -ne 1 -or
        [string]$Marker.archive -ne $ArchiveName -or
        ([string]$Marker.sha256).ToUpperInvariant() -ne $ExpectedArchiveHash) {
        Write-Verbose 'TurboLink install marker does not match the pinned archive.'
        return $null
    }

    $InstalledPlatforms = @($Marker.platforms | ForEach-Object { [string]$_ })
    if ($InstalledPlatforms.Count -eq 0) {
        Write-Verbose 'TurboLink install marker contains no platforms.'
        return $null
    }
    foreach ($Platform in $InstalledPlatforms) {
        if (-not $PlatformDirectoryMap.ContainsKey($Platform)) {
            Write-Verbose "TurboLink install marker contains an unsupported platform: $Platform"
            return $null
        }
    }

    $Records = @($Marker.files)
    if ($Records.Count -eq 0) {
        Write-Verbose 'TurboLink install marker contains no file records.'
        return $null
    }

    $SafeThirdPartyPrefix = [IO.Path]::GetFullPath($ThirdPartyRoot).TrimEnd('\') + '\'
    $RecordedPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($Record in $Records) {
        foreach ($Property in @('path', 'length', 'sha256')) {
            if ($null -eq $Record.PSObject.Properties[$Property]) {
                Write-Verbose "TurboLink install marker file record is missing property: $Property"
                return $null
            }
        }

        $RelativePath = ([string]$Record.path).Replace('/', '\').TrimStart('\')
        if ([string]::IsNullOrWhiteSpace($RelativePath) -or [IO.Path]::IsPathRooted([string]$Record.path)) {
            Write-Verbose "TurboLink install marker contains an unsafe path: $($Record.path)"
            return $null
        }
        $InstalledPath = [IO.Path]::GetFullPath((Join-Path $ThirdPartyRoot $RelativePath))
        if (-not $InstalledPath.StartsWith($SafeThirdPartyPrefix, [StringComparison]::OrdinalIgnoreCase) -or
            -not $RecordedPaths.Add($RelativePath)) {
            Write-Verbose "TurboLink install marker contains an unsafe or duplicate path: $RelativePath"
            return $null
        }
        if (-not (Test-Path -LiteralPath $InstalledPath -PathType Leaf)) {
            Write-Verbose "TurboLink installed file is missing: $InstalledPath"
            return $null
        }

        $InstalledFile = Get-Item -LiteralPath $InstalledPath
        if ($InstalledFile.Length -ne [int64]$Record.length) {
            Write-Verbose "TurboLink installed file size mismatch: $InstalledPath"
            return $null
        }
        $ActualHash = (Get-FileHash -LiteralPath $InstalledPath -Algorithm SHA256).Hash.ToUpperInvariant()
        if ($ActualHash -ne ([string]$Record.sha256).ToUpperInvariant()) {
            Write-Verbose "TurboLink installed file hash mismatch: $InstalledPath"
            return $null
        }
    }

    $Required = [Collections.Generic.List[string]]::new()
    foreach ($RelativePath in Get-CommonSentinels) {
        $Required.Add($RelativePath)
    }
    foreach ($Platform in $InstalledPlatforms) {
        foreach ($RelativePath in Get-PlatformSentinels -Platform $Platform) {
            $Required.Add($RelativePath)
        }
    }
    foreach ($RelativePath in $Required) {
        if (-not $RecordedPaths.Contains($RelativePath)) {
            Write-Verbose "TurboLink install marker does not cover required file: $RelativePath"
            return $null
        }
    }

    return $Marker
}

function Test-InstalledFiles {
    param([AllowNull()][object]$Marker)

    if ($null -eq $Marker) {
        return $false
    }
    $InstalledPlatforms = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($Platform in @($Marker.platforms)) {
        [void]$InstalledPlatforms.Add([string]$Platform)
    }
    foreach ($Platform in $Platforms) {
        if (-not $InstalledPlatforms.Contains($Platform)) {
            Write-Verbose "TurboLink platform is not registered as installed: $Platform"
            return $false
        }
    }
    return $true
}

function Test-ArchiveFile {
    if (-not (Test-Path -LiteralPath $ArchivePath -PathType Leaf)) {
        return $false
    }
    if ((Get-Item -LiteralPath $ArchivePath).Length -ne $ExpectedArchiveSize) {
        return $false
    }
    return (Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash.ToUpperInvariant() -eq $ExpectedArchiveHash
}

function Get-ValidatedArchive {
    New-Item -ItemType Directory -Path (Split-Path -Parent $ArchivePath) -Force | Out-Null

    if (Test-ArchiveFile) {
        Write-Host "Using cached TurboLink libraries: $ArchivePath"
        return $ArchivePath
    }

    $PartialPath = "$ArchivePath.partial.$([Guid]::NewGuid().ToString('N'))"
    Write-Host "Downloading $ArchiveName ($ExpectedArchiveSize bytes)..."
    try {
        $Curl = Get-Command 'curl.exe' -ErrorAction SilentlyContinue
        if ($null -ne $Curl) {
            & $Curl.Source --location --fail --retry 3 --output $PartialPath $ArchiveUrl
            if ($LASTEXITCODE -ne 0) {
                throw "curl.exe failed with exit code $LASTEXITCODE"
            }
        }
        else {
            $PreviousProgressPreference = $ProgressPreference
            $ProgressPreference = 'SilentlyContinue'
            try {
                Invoke-WebRequest -UseBasicParsing -Uri $ArchiveUrl -OutFile $PartialPath
            }
            finally {
                $ProgressPreference = $PreviousProgressPreference
            }
        }

        $DownloadedSize = (Get-Item -LiteralPath $PartialPath).Length
        if ($DownloadedSize -ne $ExpectedArchiveSize) {
            throw "Downloaded archive size mismatch. Expected $ExpectedArchiveSize, got $DownloadedSize"
        }

        $DownloadedHash = (Get-FileHash -LiteralPath $PartialPath -Algorithm SHA256).Hash.ToUpperInvariant()
        if ($DownloadedHash -ne $ExpectedArchiveHash) {
            throw "Downloaded archive SHA-256 mismatch. Expected $ExpectedArchiveHash, got $DownloadedHash"
        }

        Move-Item -LiteralPath $PartialPath -Destination $ArchivePath -Force
    }
    finally {
        if (Test-Path -LiteralPath $PartialPath) {
            Remove-Item -LiteralPath $PartialPath -Force
        }
    }

    return $ArchivePath
}

function Test-SelectedArchiveEntry {
    param([Parameter(Mandatory = $true)][string]$EntryName)

    $Parts = $EntryName.TrimEnd('/').Split('/')
    if ($Parts.Count -lt 2) {
        return $false
    }
    if ($Parts[1] -eq 'include') {
        return $true
    }
    if ($Parts[1] -eq 'bin' -and $Parts[0] -in @('grpc', 'protobuf')) {
        return $true
    }
    if ($Parts.Count -ge 3 -and $Parts[1] -eq 'lib' -and $Parts[2] -in $LibraryDirectories) {
        return $true
    }
    return $false
}

function Install-SelectedLibraries {
    param(
        [Parameter(Mandatory = $true)][string]$ValidatedArchive,
        [AllowNull()][object]$ExistingMarker
    )

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $IntermediateRoot = Join-Path $PluginRoot 'Intermediate'
    New-Item -ItemType Directory -Path $IntermediateRoot -Force | Out-Null
    $StagingRoot = Join-Path $IntermediateRoot ("ProjectM-TurboLink-" + [Guid]::NewGuid().ToString('N'))
    $StagedThirdParty = Join-Path $StagingRoot 'ThirdParty'
    $BackupRoot = Join-Path $StagingRoot 'Backup'
    New-Item -ItemType Directory -Path $StagedThirdParty -Force | Out-Null

    $InstallUnits = @(Get-InstallUnits)
    $UnitPrefixes = @($InstallUnits | ForEach-Object { $_.TrimEnd('\') + '\' })
    $CompletedSwaps = [Collections.Generic.List[object]]::new()
    $InstallCommitted = $false
    $RollbackSucceeded = $true
    $MarkerTempPath = "$InstallMarkerPath.new.$([Guid]::NewGuid().ToString('N'))"
    $MarkerBackupPath = "$InstallMarkerPath.backup.$([Guid]::NewGuid().ToString('N'))"

    try {
        $Archive = [IO.Compression.ZipFile]::OpenRead($ValidatedArchive)
        try {
            $SelectedEntries = @($Archive.Entries | Where-Object {
                -not [string]::IsNullOrEmpty($_.Name) -and (Test-SelectedArchiveEntry -EntryName $_.FullName)
            })
            if ($SelectedEntries.Count -eq 0) {
                throw 'No matching TurboLink library files were found in the archive.'
            }

            $SafeStagedPrefix = [IO.Path]::GetFullPath($StagedThirdParty).TrimEnd('\') + '\'
            foreach ($Entry in $SelectedEntries) {
                $RelativePath = $Entry.FullName.Replace('/', [IO.Path]::DirectorySeparatorChar)
                $Destination = [IO.Path]::GetFullPath((Join-Path $StagedThirdParty $RelativePath))
                if (-not $Destination.StartsWith($SafeStagedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
                    throw "Unsafe archive entry path: $($Entry.FullName)"
                }
                New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
                [IO.Compression.ZipFileExtensions]::ExtractToFile($Entry, $Destination, $true)
                if ((Get-Item -LiteralPath $Destination).Length -ne $Entry.Length) {
                    throw "Extracted file size mismatch: $($Entry.FullName)"
                }
            }
        }
        finally {
            $Archive.Dispose()
        }

        foreach ($Unit in $InstallUnits) {
            if (-not (Test-Path -LiteralPath (Join-Path $StagedThirdParty $Unit) -PathType Container)) {
                throw "Pinned archive does not contain required install unit: $Unit"
            }
        }

        $StagedFiles = @(Get-ChildItem -LiteralPath $StagedThirdParty -Recurse -File)
        $SelectedRecords = [Collections.Generic.List[object]]::new()
        foreach ($File in $StagedFiles) {
            $RelativePath = $File.FullName.Substring($StagedThirdParty.Length).TrimStart('\', '/')
            $SelectedRecords.Add([pscustomobject][ordered]@{
                path = $RelativePath
                length = [int64]$File.Length
                sha256 = (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash.ToUpperInvariant()
            })
        }

        $RecordMap = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
        if ($null -ne $ExistingMarker) {
            foreach ($Record in @($ExistingMarker.files)) {
                $RelativePath = ([string]$Record.path).Replace('/', '\').TrimStart('\')
                $IsReplaced = $false
                foreach ($Prefix in $UnitPrefixes) {
                    if ($RelativePath.StartsWith($Prefix, [StringComparison]::OrdinalIgnoreCase)) {
                        $IsReplaced = $true
                        break
                    }
                }
                if (-not $IsReplaced) {
                    $RecordMap[$RelativePath] = [pscustomobject][ordered]@{
                        path = $RelativePath
                        length = [int64]$Record.length
                        sha256 = ([string]$Record.sha256).ToUpperInvariant()
                    }
                }
            }
        }
        foreach ($Record in $SelectedRecords) {
            $RecordMap[[string]$Record.path] = $Record
        }

        $InstalledPlatformSet = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        if ($null -ne $ExistingMarker) {
            foreach ($Platform in @($ExistingMarker.platforms)) {
                [void]$InstalledPlatformSet.Add([string]$Platform)
            }
        }
        foreach ($Platform in $Platforms) {
            [void]$InstalledPlatformSet.Add($Platform)
        }

        New-Item -ItemType Directory -Path $ThirdPartyRoot -Force | Out-Null
        try {
            foreach ($Unit in $InstallUnits) {
                $Source = [IO.Path]::GetFullPath((Join-Path $StagedThirdParty $Unit))
                $Destination = [IO.Path]::GetFullPath((Join-Path $ThirdPartyRoot $Unit))
                $Backup = [IO.Path]::GetFullPath((Join-Path $BackupRoot $Unit))
                $HadOriginal = Test-Path -LiteralPath $Destination -PathType Container
                if ((Test-Path -LiteralPath $Destination) -and -not $HadOriginal) {
                    throw "TurboLink install unit is not a directory: $Destination"
                }

                New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
                if ($HadOriginal) {
                    New-Item -ItemType Directory -Path (Split-Path -Parent $Backup) -Force | Out-Null
                    [IO.Directory]::Move($Destination, $Backup)
                }
                $SwapRecord = [pscustomobject]@{
                    destination = $Destination
                    backup = $Backup
                    hadOriginal = $HadOriginal
                    installedNew = $false
                }
                $CompletedSwaps.Add($SwapRecord)
                [IO.Directory]::Move($Source, $Destination)
                $SwapRecord.installedNew = $true
            }

            $MergedPlatforms = @($InstalledPlatformSet | Sort-Object)
            $MergedRecords = @($RecordMap.Values | Sort-Object -Property path)
            $MarkerJson = [ordered]@{
                schemaVersion = 1
                archive = $ArchiveName
                sha256 = $ExpectedArchiveHash
                platforms = $MergedPlatforms
                files = $MergedRecords
                installedAtUtc = [DateTime]::UtcNow.ToString('o')
            } | ConvertTo-Json -Depth 5
            [IO.File]::WriteAllText($MarkerTempPath, $MarkerJson + "`n", [Text.UTF8Encoding]::new($false))
            if (Test-Path -LiteralPath $InstallMarkerPath -PathType Leaf) {
                [IO.File]::Replace($MarkerTempPath, $InstallMarkerPath, $MarkerBackupPath)
            }
            else {
                [IO.File]::Move($MarkerTempPath, $InstallMarkerPath)
            }
            $InstallCommitted = $true
        }
        finally {
            if (-not $InstallCommitted -and $CompletedSwaps.Count -gt 0) {
                try {
                    for ($Index = $CompletedSwaps.Count - 1; $Index -ge 0; $Index--) {
                        $Swap = $CompletedSwaps[$Index]
                        if ([bool]$Swap.installedNew -and (Test-Path -LiteralPath $Swap.destination -PathType Container)) {
                            [IO.Directory]::Delete([string]$Swap.destination, $true)
                        }
                        if ([bool]$Swap.hadOriginal -and (Test-Path -LiteralPath $Swap.backup -PathType Container)) {
                            if (Test-Path -LiteralPath $Swap.destination) {
                                throw "Cannot restore TurboLink backup because the destination is occupied: $($Swap.destination)"
                            }
                            New-Item -ItemType Directory -Path (Split-Path -Parent $Swap.destination) -Force | Out-Null
                            [IO.Directory]::Move([string]$Swap.backup, [string]$Swap.destination)
                        }
                    }
                }
                catch {
                    $RollbackSucceeded = $false
                    Write-Warning "TurboLink install rollback was incomplete. Recovery files were preserved at: $StagingRoot"
                    throw
                }
            }
        }

        if (Test-Path -LiteralPath $MarkerBackupPath -PathType Leaf) {
            Remove-Item -LiteralPath $MarkerBackupPath -Force -ErrorAction SilentlyContinue
        }
        Write-Host "Installed $($StagedFiles.Count) TurboLink files atomically for: $($Platforms -join ', ')"
    }
    finally {
        if (Test-Path -LiteralPath $MarkerTempPath -PathType Leaf) {
            Remove-Item -LiteralPath $MarkerTempPath -Force -ErrorAction SilentlyContinue
        }
        $ResolvedStagingRoot = [IO.Path]::GetFullPath($StagingRoot)
        $SafeIntermediatePrefix = [IO.Path]::GetFullPath($IntermediateRoot).TrimEnd('\') + '\'
        if ($ResolvedStagingRoot.StartsWith($SafeIntermediatePrefix, [StringComparison]::OrdinalIgnoreCase) -and
            (Split-Path -Leaf $ResolvedStagingRoot).StartsWith('ProjectM-TurboLink-', [StringComparison]::Ordinal)) {
            if ($RollbackSucceeded -and (Test-Path -LiteralPath $ResolvedStagingRoot -PathType Container)) {
                [IO.Directory]::Delete($ResolvedStagingRoot, $true)
            }
        }
        else {
            throw "Refusing to remove unexpected staging path: $ResolvedStagingRoot"
        }
    }
}

$GeneratorPath = Join-Path $PluginRoot 'Tools\protoc-gen-turbolink.exe'
Assert-FileHash -Path $GeneratorPath -ExpectedHash ([string]$Lock.turboLink.codeGeneratorSha256) -Description 'TurboLink code generator'

$ExistingMarker = Get-ValidatedInstallMarker
$AlreadyInstalled = Test-InstalledFiles -Marker $ExistingMarker
if ($ValidateOnly) {
    if (-not $AlreadyInstalled) {
        throw "TurboLink libraries are missing, unregistered, or corrupt for: $($Platforms -join ', '). Run this script without -ValidateOnly to restore them."
    }
}
elseif ($ForceInstall -or -not $AlreadyInstalled) {
    $ValidatedArchive = Get-ValidatedArchive
    Install-SelectedLibraries -ValidatedArchive $ValidatedArchive -ExistingMarker $ExistingMarker
    $ExistingMarker = Get-ValidatedInstallMarker
    $AlreadyInstalled = Test-InstalledFiles -Marker $ExistingMarker
}
else {
    Write-Host "TurboLink libraries are already installed for: $($Platforms -join ', ')"
}

$ProtocPath = Join-Path $ThirdPartyRoot 'protobuf\bin\protoc.exe'
$GrpcPluginPath = Join-Path $ThirdPartyRoot 'grpc\bin\grpc_cpp_plugin.exe'
Assert-FileHash -Path $ProtocPath -ExpectedHash ([string]$Lock.libraries.protocSha256) -Description 'protoc.exe'
Assert-FileHash -Path $GrpcPluginPath -ExpectedHash ([string]$Lock.libraries.grpcCppPluginSha256) -Description 'grpc_cpp_plugin.exe'

if (-not $AlreadyInstalled) {
    throw "TurboLink installation validation failed for: $($Platforms -join ', ')"
}

Write-Host "TurboLink dependency validation succeeded: $($Platforms -join ', ')"

if (-not $ValidateOnly -and -not $SkipProtoGeneration) {
    & $GenerateScript -ProjectRoot $ProjectRoot
}
