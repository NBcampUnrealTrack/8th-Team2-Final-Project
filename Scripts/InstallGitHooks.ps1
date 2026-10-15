# Installs the ProjectM git hooks into this clone.
#
# Scripts/GitHooks/proto-autogen.sh regenerates the TurboLink gRPC code when a pull
# or branch switch changes Protos/*.proto (see that script's header). Git does not
# version .git/hooks, so every clone installs it once: Scripts/Setup.bat does it for
# new clones, existing clones run Scripts/InstallGitHooks.bat.
#
# Hooks that already exist (Git LFS) are kept as they are. One call line is inserted
# right after the shebang, so the LFS command stays last and still decides the hook's
# exit status. Re-running is safe; -Uninstall removes only the inserted lines.
[CmdletBinding()]
param(
    [switch]$Uninstall
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Marker = '# projectm-proto-autogen'
$HookScript = 'Scripts/GitHooks/proto-autogen.sh'
$HookNames = @('post-merge', 'post-rewrite', 'post-checkout', 'post-commit')

# Honors core.hooksPath if someone set it.
$HooksDir = & git -C $ProjectRoot rev-parse --git-path hooks
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($HooksDir)) {
    throw "Not a git clone: $ProjectRoot"
}
$HooksDir = [IO.Path]::GetFullPath([IO.Path]::Combine($ProjectRoot, $HooksDir.Trim()))
New-Item -ItemType Directory -Path $HooksDir -Force | Out-Null

foreach ($HookName in $HookNames) {
    $HookPath = Join-Path $HooksDir $HookName
    $Existed = Test-Path -LiteralPath $HookPath -PathType Leaf

    # Drop any line from an earlier install first, so re-running refreshes it.
    $Lines = [Collections.Generic.List[string]]::new()
    if ($Existed) {
        foreach ($Line in ([IO.File]::ReadAllText($HookPath) -split "`n")) {
            if (-not $Line.Contains($Marker)) {
                $Lines.Add($Line)
            }
        }
    }

    if ($Uninstall) {
        if (-not $Existed) {
            continue
        }
        # A file holding nothing but a shebang was created by this script.
        $Remaining = @($Lines | Where-Object { $_.Trim() -ne '' -and -not $_.StartsWith('#!') })
        if ($Remaining.Count -eq 0) {
            Remove-Item -LiteralPath $HookPath -Force
            Write-Host "  removed  $HookName"
            continue
        }
    }
    else {
        # 'if' rather than '&&': a missing script (older branch) must leave status 0.
        $Call = 'if [ -f {0} ]; then sh {0} {1} "$@"; fi {2}' -f $HookScript, $HookName, $Marker
        if ($Lines.Count -gt 0 -and $Lines[0].StartsWith('#!')) {
            $Lines.Insert(1, $Call)
        }
        else {
            # Git for Windows only runs hooks that start with a shebang.
            $Lines.Insert(0, $Call)
            $Lines.Insert(0, '#!/bin/sh')
            if ($Lines[$Lines.Count - 1] -ne '') {
                $Lines.Add('')
            }
        }
    }

    # LF, no BOM: sh chokes on either.
    [IO.File]::WriteAllText($HookPath, ($Lines -join "`n"), [Text.UTF8Encoding]::new($false))
    Write-Host ("  {0}  {1}" -f $(if ($Uninstall) { 'cleaned' } elseif ($Existed) { 'updated' } else { 'created' }), $HookName)
}

if ($Uninstall) {
    Write-Host 'ProjectM git hooks removed.'
    return
}

# Bring the generated code up to date now instead of at the next pull. Git runs
# '!' aliases through its own sh, so there is no sh.exe to locate here.
& git -C $ProjectRoot -c "alias.projectm-proto-sync=!sh $HookScript sync" projectm-proto-sync
if ($LASTEXITCODE -ne 0) {
    throw 'Initial proto check failed.'
}
Write-Host "ProjectM git hooks installed in $HooksDir"
