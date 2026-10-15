# Generates TurboLink C++ bindings for the .proto contracts shared with the server.
#
# There is more than one proto: game.proto (game server) and chat.proto (chat server).
# All of them are generated in a single run so the output manifest is their union.
# Generating one at a time would mark every other proto's output as obsolete,
# which then gets deleted by -Prune.
[CmdletBinding()]
param(
    [string]$ProjectRoot,
    [Alias('ProtoFile')]
    [string[]]$ProtoFiles = @('Protos\game.proto', 'Protos\chat.proto'),
    [switch]$Check,
    [switch]$Prune
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}

$ProjectRoot = [IO.Path]::GetFullPath($ProjectRoot)
$PluginRoot = Join-Path $ProjectRoot 'Plugins\TurboLink'
$ModuleRoot = Join-Path $PluginRoot 'Source\TurboLinkGrpc'
$GeneratedManifestPath = Join-Path $ModuleRoot '.projectm-generated-files.json'
$ThirdPartyRoot = Join-Path $PluginRoot 'Source\ThirdParty'
$LockPath = Join-Path $PSScriptRoot 'TurboLink\turbolink.lock.json'

if ($null -eq $ProtoFiles -or $ProtoFiles.Count -eq 0) {
    throw 'At least one proto file is required.'
}

$ResolvedProtoFiles = [Collections.Generic.List[string]]::new()
$SeenProtoPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($ProtoEntry in $ProtoFiles) {
    $Candidate = [string]$ProtoEntry
    if (-not [IO.Path]::IsPathRooted($Candidate)) {
        $Candidate = Join-Path $ProjectRoot $Candidate
    }
    $Candidate = [IO.Path]::GetFullPath($Candidate)

    if (-not (Test-Path -LiteralPath $Candidate -PathType Leaf)) {
        throw "Proto file not found: $Candidate"
    }
    if (-not $SeenProtoPaths.Add($Candidate)) {
        continue # tolerate the same proto being passed twice
    }
    $ResolvedProtoFiles.Add($Candidate)
}
if (-not (Test-Path -LiteralPath $LockPath -PathType Leaf)) {
    throw "TurboLink lock file not found: $LockPath"
}
if (-not (Test-Path -LiteralPath $ModuleRoot -PathType Container)) {
    throw "TurboLink module source is missing: $ModuleRoot"
}

$Lock = Get-Content -Raw -Encoding UTF8 $LockPath | ConvertFrom-Json
$ProtocPath = Join-Path $ThirdPartyRoot 'protobuf\bin\protoc.exe'
$GrpcPluginPath = Join-Path $ThirdPartyRoot 'grpc\bin\grpc_cpp_plugin.exe'
$TurboLinkGeneratorPath = Join-Path $PluginRoot 'Tools\protoc-gen-turbolink.exe'
$ProtobufIncludePath = Join-Path $ThirdPartyRoot 'protobuf\include'
$FixProtoHeaderPath = Join-Path $PluginRoot 'Tools\fix_proto_h.txt'
$FixProtoCppPath = Join-Path $PluginRoot 'Tools\fix_proto_cpp.txt'

function Assert-ToolHash {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$ExpectedHash,
        [Parameter(Mandatory = $true)][string]$Name
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Name is missing. Run Scripts/SetupTurboLink.ps1 first. File: $Path"
    }
    $ActualHash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($ActualHash -ne $ExpectedHash.ToUpperInvariant()) {
        throw "$Name hash mismatch. Run Scripts/SetupTurboLink.ps1 to restore the pinned tool."
    }
}

function Write-Utf8NoBom {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Content
    )
    [IO.File]::WriteAllText($Path, $Content, [Text.UTF8Encoding]::new($false))
}

function Normalize-LineEndings {
    param([Parameter(Mandatory = $true)][string]$Text)
    return $Text.Replace("`r`n", "`n").Replace("`r", "`n")
}

Assert-ToolHash -Path $TurboLinkGeneratorPath -ExpectedHash ([string]$Lock.turboLink.codeGeneratorSha256) -Name 'protoc-gen-turbolink.exe'
Assert-ToolHash -Path $ProtocPath -ExpectedHash ([string]$Lock.libraries.protocSha256) -Name 'protoc.exe'
Assert-ToolHash -Path $GrpcPluginPath -ExpectedHash ([string]$Lock.libraries.grpcCppPluginSha256) -Name 'grpc_cpp_plugin.exe'

foreach ($RequiredPath in @($ProtobufIncludePath, $FixProtoHeaderPath, $FixProtoCppPath)) {
    if (-not (Test-Path -LiteralPath $RequiredPath)) {
        throw "Required TurboLink file is missing: $RequiredPath"
    }
}

$IntermediateRoot = Join-Path $PluginRoot 'Intermediate'
New-Item -ItemType Directory -Path $IntermediateRoot -Force | Out-Null
$TempOutput = Join-Path $IntermediateRoot ("ProjectM-ProtoGen-" + [Guid]::NewGuid().ToString('N'))
$PrivatePbOutput = Join-Path $TempOutput 'Private\pb'
New-Item -ItemType Directory -Path $PrivatePbOutput -Force | Out-Null

$ChangedFiles = [Collections.Generic.List[string]]::new()
$ExtraFiles = [Collections.Generic.List[string]]::new()
$ManifestChanged = $false

try {
    $HeaderPrefix = [IO.File]::ReadAllText($FixProtoHeaderPath)
    $CppPrefix = [IO.File]::ReadAllText($FixProtoCppPath)

    # Run protoc once per proto, but collect every output into the SAME temp tree,
    # so the manifest is their union and generating one proto never orphans another.
    $ProducedByProto = [Collections.Generic.Dictionary[string, string]]::new([StringComparer]::OrdinalIgnoreCase)

    foreach ($CurrentProtoFile in $ResolvedProtoFiles) {
        $ProtoDirectory = Split-Path -Parent $CurrentProtoFile
        $ProtoName = Split-Path -Leaf $CurrentProtoFile

        # Snapshot the tree first so we can tell what this proto created vs. overwrote.
        $BeforeHashes = [Collections.Generic.Dictionary[string, string]]::new([StringComparer]::OrdinalIgnoreCase)
        if (Test-Path -LiteralPath $TempOutput -PathType Container) {
            foreach ($Existing in Get-ChildItem -LiteralPath $TempOutput -Recurse -File) {
                $ExistingRelative = $Existing.FullName.Substring($TempOutput.Length).TrimStart('\', '/')
                $BeforeHashes[$ExistingRelative] = (Get-FileHash -LiteralPath $Existing.FullName -Algorithm SHA256).Hash
            }
        }

        Push-Location $ProtoDirectory
        try {
            & $ProtocPath `
                "--proto_path=$ProtobufIncludePath" `
                "--proto_path=$ProtoDirectory" `
                "--cpp_out=$PrivatePbOutput" `
                "--plugin=protoc-gen-grpc=$GrpcPluginPath" `
                "--grpc_out=$PrivatePbOutput" `
                "--plugin=protoc-gen-turbolink=$TurboLinkGeneratorPath" `
                "--turbolink_out=$TempOutput" `
                '--turbolink_opt=GenerateJsonCode=true' `
                $ProtoName
            if ($LASTEXITCODE -ne 0) {
                throw "protoc generation failed for ${ProtoName} with exit code $LASTEXITCODE"
            }
        }
        finally {
            Pop-Location
        }

        $ProtoBaseName = [IO.Path]::GetFileNameWithoutExtension($ProtoName)
        $GeneratedPbHeader = Join-Path $PrivatePbOutput "$ProtoBaseName.pb.h"
        $GeneratedPbCpp = Join-Path $PrivatePbOutput "$ProtoBaseName.pb.cc"
        if (-not (Test-Path -LiteralPath $GeneratedPbHeader) -or -not (Test-Path -LiteralPath $GeneratedPbCpp)) {
            throw "protoc did not generate the expected protobuf C++ files for $ProtoName."
        }

        Write-Utf8NoBom -Path $GeneratedPbHeader -Content ($HeaderPrefix + [IO.File]::ReadAllText($GeneratedPbHeader))
        Write-Utf8NoBom -Path $GeneratedPbCpp -Content ($CppPrefix + [IO.File]::ReadAllText($GeneratedPbCpp))

        # If two protos emit the same path with DIFFERENT content, the later run silently
        # wins. Distinct proto packages keep them apart, but a collision would be a
        # very hard bug to trace, so fail loudly here. Hashes tell "untouched" from
        # "overwritten" - a file left alone by this proto keeps its previous hash.
        foreach ($Produced in Get-ChildItem -LiteralPath $TempOutput -Recurse -File) {
            $RelativePath = $Produced.FullName.Substring($TempOutput.Length).TrimStart('\', '/')
            $CurrentHash = (Get-FileHash -LiteralPath $Produced.FullName -Algorithm SHA256).Hash

            $PreviousHash = $null
            if ($BeforeHashes.TryGetValue($RelativePath, [ref]$PreviousHash)) {
                if ($PreviousHash -eq $CurrentHash) {
                    continue # untouched by this proto
                }
                $Owner = $null
                if ($ProducedByProto.TryGetValue($RelativePath, [ref]$Owner) -and $Owner -ne $ProtoName) {
                    throw "Proto output collision: '$RelativePath' is produced by both $Owner and $ProtoName. Change the proto package so the outputs do not overlap."
                }
            }
            $ProducedByProto[$RelativePath] = $ProtoName
        }
    }

    $MetadataPatchCount = 0
    foreach ($Header in Get-ChildItem -LiteralPath (Join-Path $TempOutput 'Public') -Recurse -File -Filter '*.h') {
        $Text = [IO.File]::ReadAllText($Header.FullName)
        $Pattern = '(HasNative(?:Make|Break)\s*=\s*")TurboLinkGrpc\.'
        $Matches = [regex]::Matches($Text, $Pattern).Count
        if ($Matches -gt 0) {
            $Text = [regex]::Replace($Text, $Pattern, '${1}/Script/TurboLinkGrpc.')
            $MetadataPatchCount += $Matches
            Write-Utf8NoBom -Path $Header.FullName -Content $Text
        }
    }
    if ($MetadataPatchCount -eq 0) {
        throw 'Expected TurboLink HasNativeMake/HasNativeBreak metadata was not found.'
    }

    $PrivateIncludePatchCount = 0
    foreach ($Header in Get-ChildItem -LiteralPath (Join-Path $TempOutput 'Private') -Recurse -File -Filter '*_Private.h') {
        $Text = [IO.File]::ReadAllText($Header.FullName)
        $Pattern = '(?m)^#include "[^"]+_Private\.generated\.h"\r?\n'
        $Matches = [regex]::Matches($Text, $Pattern).Count
        if ($Matches -gt 0) {
            $Text = [regex]::Replace($Text, $Pattern, '')
            $PrivateIncludePatchCount += $Matches
            Write-Utf8NoBom -Path $Header.FullName -Content $Text
        }
    }
    if ($PrivateIncludePatchCount -eq 0) {
        throw 'Expected invalid TurboLink private generated.h include was not found.'
    }

    $OptionalPatchCount = 0
    $OptionalPattern = '(?ms)^    switch\(in->_(?<field>[A-Za-z0-9_]+)_case\(\)\)\r?\n    \{\r?\n    case [^\r\n]+:\r?\n(?<body>.*?)^        break;\r?\n^    \}'
    foreach ($MarshalingFile in Get-ChildItem -LiteralPath (Join-Path $TempOutput 'Private') -Recurse -File -Filter '*Marshaling.cpp') {
        $Text = [IO.File]::ReadAllText($MarshalingFile.FullName)
        $Matches = [regex]::Matches($Text, $OptionalPattern)
        if ($Matches.Count -gt 0) {
            $Text = [regex]::Replace($Text, $OptionalPattern, {
                param($Match)
                $Field = $Match.Groups['field'].Value
                $Body = $Match.Groups['body'].Value
                return "    if(in->has_$Field())`n    {`n$Body    }"
            })
            $OptionalPatchCount += $Matches.Count
            Write-Utf8NoBom -Path $MarshalingFile.FullName -Content $Text
        }

        $RemainingInvalidCases = [regex]::Matches($Text, 'switch\(in->_[A-Za-z0-9_]+_case\(\)\)')
        if ($RemainingInvalidCases.Count -gt 0) {
            throw "Unpatched proto3 optional marshaling remains in $($MarshalingFile.FullName)"
        }
    }
    if ($OptionalPatchCount -eq 0) {
        throw 'Expected proto3 optional marshaling blocks were not found.'
    }

    $GeneratedFiles = @(Get-ChildItem -LiteralPath $TempOutput -Recurse -File | Where-Object {
        $_.Extension -in @('.h', '.cpp', '.cc')
    })
    if ($GeneratedFiles.Count -eq 0) {
        throw 'TurboLink produced no generated source files.'
    }

    foreach ($File in $GeneratedFiles) {
        $RelativePath = $File.FullName.Substring($TempOutput.Length).TrimStart('\', '/')
        if (-not $RelativePath.StartsWith('Private\pb\', [StringComparison]::OrdinalIgnoreCase)) {
            $Normalized = Normalize-LineEndings -Text ([IO.File]::ReadAllText($File.FullName))
            Write-Utf8NoBom -Path $File.FullName -Content $Normalized
        }
    }

    $GeneratedRelativePaths = @($GeneratedFiles | ForEach-Object {
        $_.FullName.Substring($TempOutput.Length).TrimStart('\', '/')
    } | Sort-Object)
    $GeneratedPathSet = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($RelativePath in $GeneratedRelativePaths) {
        [void]$GeneratedPathSet.Add($RelativePath)
    }

    $ExpectedManifest = [ordered]@{
        schemaVersion = 1
        files = $GeneratedRelativePaths
    } | ConvertTo-Json -Depth 3
    $ExpectedManifest += "`n"

    $PreviousGeneratedPaths = [Collections.Generic.List[string]]::new()
    if (Test-Path -LiteralPath $GeneratedManifestPath -PathType Leaf) {
        try {
            $ExistingManifestText = [IO.File]::ReadAllText($GeneratedManifestPath)
            $ExistingManifest = $ExistingManifestText | ConvertFrom-Json
            if ($null -eq $ExistingManifest.PSObject.Properties['schemaVersion'] -or
                $null -eq $ExistingManifest.PSObject.Properties['files'] -or
                [int]$ExistingManifest.schemaVersion -ne 1) {
                throw 'unsupported or incomplete manifest schema'
            }

            $SafeModulePrefix = [IO.Path]::GetFullPath($ModuleRoot).TrimEnd('\') + '\'
            $SeenManifestPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
            foreach ($ManifestPath in @($ExistingManifest.files)) {
                $RelativePath = ([string]$ManifestPath).Replace('/', '\').TrimStart('\')
                $ResolvedPath = [IO.Path]::GetFullPath((Join-Path $ModuleRoot $RelativePath))
                if ([string]::IsNullOrWhiteSpace($RelativePath) -or
                    [IO.Path]::IsPathRooted([string]$ManifestPath) -or
                    -not $ResolvedPath.StartsWith($SafeModulePrefix, [StringComparison]::OrdinalIgnoreCase) -or
                    -not $SeenManifestPaths.Add($RelativePath)) {
                    throw "unsafe or duplicate generated path: $ManifestPath"
                }
                $PreviousGeneratedPaths.Add($RelativePath)
            }
            $SortedPreviousPaths = @($PreviousGeneratedPaths | Sort-Object)
            $ManifestChanged = $SortedPreviousPaths.Count -ne $GeneratedRelativePaths.Count
            if (-not $ManifestChanged) {
                for ($Index = 0; $Index -lt $GeneratedRelativePaths.Count; $Index++) {
                    if (-not [string]::Equals(
                        [string]$SortedPreviousPaths[$Index],
                        [string]$GeneratedRelativePaths[$Index],
                        [StringComparison]::Ordinal
                    )) {
                        $ManifestChanged = $true
                        break
                    }
                }
            }
        }
        catch {
            throw "Generated-file manifest is invalid: $GeneratedManifestPath. $($_.Exception.Message)"
        }
    }
    else {
        $ManifestChanged = $true
    }

    $ExtraPathSet = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($RelativePath in $PreviousGeneratedPaths) {
        if (-not $GeneratedPathSet.Contains($RelativePath)) {
            $ExistingPath = Join-Path $ModuleRoot $RelativePath
            if ((Test-Path -LiteralPath $ExistingPath -PathType Leaf) -and $ExtraPathSet.Add($RelativePath)) {
                $ExtraFiles.Add($RelativePath)
            }
        }
    }

    # Compatibility scan for repositories created before the manifest was introduced.
    # Scan directories are derived from what was actually generated this run, so adding a
    # proto does not require editing a hardcoded list (forgetting to would silently leave
    # stale output behind). TurboLink names directories after the proto package:
    # rev.game -> SRevGame, rev.chat -> SRevChat.
    $ScanDirectories = [Collections.Generic.List[string]]::new()
    foreach ($Legacy in @('Private\SRevGame', 'Private\pb', 'Public\SRevGame')) {
        $ScanDirectories.Add($Legacy)
    }
    foreach ($Scope in @('Private', 'Public')) {
        $ScopeRoot = Join-Path $TempOutput $Scope
        if (-not (Test-Path -LiteralPath $ScopeRoot -PathType Container)) {
            continue
        }
        foreach ($ScopeDirectory in Get-ChildItem -LiteralPath $ScopeRoot -Directory) {
            $Candidate = Join-Path $Scope $ScopeDirectory.Name
            if (-not $ScanDirectories.Contains($Candidate)) {
                $ScanDirectories.Add($Candidate)
            }
        }
    }

    foreach ($GeneratedDirectory in $ScanDirectories) {
        $ExistingDirectory = Join-Path $ModuleRoot $GeneratedDirectory
        if (Test-Path -LiteralPath $ExistingDirectory) {
            foreach ($ExistingFile in Get-ChildItem -LiteralPath $ExistingDirectory -Recurse -File) {
                $RelativePath = $ExistingFile.FullName.Substring($ModuleRoot.Length).TrimStart('\', '/')
                if (-not $GeneratedPathSet.Contains($RelativePath) -and $ExtraPathSet.Add($RelativePath)) {
                    $ExtraFiles.Add($RelativePath)
                }
            }
        }
    }

    if ($ExtraFiles.Count -gt 0 -and -not $Prune) {
        throw ("Generated output contains obsolete files. Re-run with -Prune after review:`n - " + ($ExtraFiles -join "`n - "))
    }

    foreach ($GeneratedFile in $GeneratedFiles) {
        $RelativePath = $GeneratedFile.FullName.Substring($TempOutput.Length).TrimStart('\', '/')
        $Destination = Join-Path $ModuleRoot $RelativePath
        $NeedsUpdate = -not (Test-Path -LiteralPath $Destination -PathType Leaf)
        if (-not $NeedsUpdate) {
            $NeedsUpdate = (Get-FileHash -LiteralPath $GeneratedFile.FullName -Algorithm SHA256).Hash -ne
                (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash
        }
        if ($NeedsUpdate) {
            $ChangedFiles.Add($RelativePath)
            if (-not $Check) {
                New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
                Copy-Item -LiteralPath $GeneratedFile.FullName -Destination $Destination -Force
            }
        }
    }

    if ($Prune -and -not $Check) {
        foreach ($RelativePath in $ExtraFiles) {
            $ObsoletePath = [IO.Path]::GetFullPath((Join-Path $ModuleRoot $RelativePath))
            $SafeModulePrefix = [IO.Path]::GetFullPath($ModuleRoot).TrimEnd('\') + '\'
            if (-not $ObsoletePath.StartsWith($SafeModulePrefix, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Refusing to remove unexpected generated path: $ObsoletePath"
            }
            Remove-Item -LiteralPath $ObsoletePath -Force
        }
    }

    if (-not $Check -and $ManifestChanged) {
        Write-Utf8NoBom -Path $GeneratedManifestPath -Content $ExpectedManifest
    }

    if ($Check -and ($ChangedFiles.Count -gt 0 -or $ExtraFiles.Count -gt 0 -or $ManifestChanged)) {
        $Drift = @($ChangedFiles) + @($ExtraFiles)
        if ($ManifestChanged) {
            $Drift += '.projectm-generated-files.json'
        }
        throw ("Generated proto code is out of date:`n - " + ($Drift -join "`n - "))
    }

    if ($ChangedFiles.Count -eq 0 -and $ExtraFiles.Count -eq 0 -and -not $ManifestChanged) {
        Write-Host "Proto code is up to date ($($ResolvedProtoFiles.Count) protos, $($GeneratedFiles.Count) files; metadata fixes: $MetadataPatchCount; optional fixes: $OptionalPatchCount)."
    }
    elseif (-not $Check) {
        Write-Host "Generated $($ResolvedProtoFiles.Count) protos: updated $($ChangedFiles.Count) files; removed $($ExtraFiles.Count) obsolete files; manifest updated: $ManifestChanged."
    }
}
finally {
    $ResolvedTempOutput = [IO.Path]::GetFullPath($TempOutput)
    $SafeIntermediatePrefix = [IO.Path]::GetFullPath($IntermediateRoot).TrimEnd('\') + '\'
    if ($ResolvedTempOutput.StartsWith($SafeIntermediatePrefix, [StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $ResolvedTempOutput).StartsWith('ProjectM-ProtoGen-', [StringComparison]::Ordinal)) {
        if (Test-Path -LiteralPath $ResolvedTempOutput) {
            Remove-Item -LiteralPath $ResolvedTempOutput -Recurse -Force
        }
    }
    else {
        throw "Refusing to remove unexpected proto generation path: $ResolvedTempOutput"
    }
}
