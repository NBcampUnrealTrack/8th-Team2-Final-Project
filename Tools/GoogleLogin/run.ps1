# Google login screen steps.  .\run.ps1 -Steps login,apply
#   login  = build /Game/Art/UI/Login/WBP_Login_Google from nothing (headless; needs the ProjectMEditor build with Auth/ and Core/NULoginWidget)
#   apply  = DefaultGame.ini: the login game mode uses the new widget (LoginWidgetOverride) + cook folder
#   revert = remove those lines (the old WBP_Login comes back)
# Builds and captures reuse the creation pipeline: ..\CharacterCreateGlass\run.ps1 -Steps build / shot -Map /Game/Maps/L_LoginLevel
param([string[]]$Steps = @('login', 'apply'))
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out = Join-Path $root 'Saved\GoogleLogin'
New-Item -ItemType Directory -Force -Path $out | Out-Null

function Headless([string]$Script) {
    $ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    $log = Join-Path $out ($Script + '.log')
    $list = @(('"' + (Join-Path $root 'ProjectM.uproject') + '"'), '-run=pythonscript', ('-script="' + (Join-Path $PSScriptRoot ($Script + '.py')) + '"'), '-unattended', '-nop4', '-nosplash', '-stdout', '-FullStdOutLogOutput')
    $p = Start-Process -FilePath $ue -ArgumentList $list -RedirectStandardOutput $log -PassThru -NoNewWindow
    if (-not $p.WaitForExit(840000)) { Stop-Process -Id $p.Id -Force; "$Script TIMEOUT" }
    $hits = Select-String -Path $log -Pattern '[LOGIN]', '[CCG]', 'LogPython: Error', 'Traceback', 'AssertionError' -SimpleMatch | Where-Object { $_.Line -notmatch 'LogInit' } | Select-Object -First 30
    foreach ($h in $hits) { $line = $h.Line; $i = [Math]::Max(0, $line.IndexOf('LogPython')); $line.Substring($i, [Math]::Min(260, $line.Length - $i)) }
    $ok = [bool](Select-String -Path $log -Pattern '[LOGIN] DONE' -SimpleMatch -Quiet)
    "$Script " + $(if ($ok) { 'OK' } else { 'FAILED' })
}

foreach ($step in $Steps) {
    switch ($step) {
        'login' {
            $file = Join-Path $root 'Content\Art\UI\Login\WBP_Login_Google.uasset'
            if (Test-Path $file) { Remove-Item $file -ErrorAction Stop }   # rebuilt from nothing (stale variable ids otherwise)
            Headless 'apply_login'
        }
        'apply' { python (Join-Path $PSScriptRoot 'apply_ini.py') }
        'revert' { python (Join-Path $PSScriptRoot 'apply_ini.py') --revert }
    }
}
