# Runs the character-creation glass pipeline steps in order.  .\run.ps1 -Steps build,stage,ui,shot [-Shot name]
# build = editor module, stage = level + backdrop, hair = hair-tint materials, ui = creation widgets + game modes,
# fx = time-driven UI materials, dialog = confirm dialogs, select = select-screen widgets + game modes, shot = play-mode capture with UI.
# -States states.json captures several screen states in one launch (see states.json); otherwise one shot named -Shot.
# -W / -H set the play window (the floating window adds 2 px of height: 1078 gives a 1080 viewport).
param([string[]]$Steps = @('stage', 'ui', 'shot'), [string]$Shot = 'glass', [string]$Map = '/Game/Developers/dbals/Map/L_CharacterCreate', [string]$GamePy = '', [string]$States = '', [int]$W = 1920, [int]$H = 1078)
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out = Join-Path $root 'Saved\CharacterCreateGlass'
New-Item -ItemType Directory -Force -Path $out | Out-Null

function Headless([string]$Script) {
    $ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    $log = Join-Path $out ($Script + '.log')
    $list = @(('"' + (Join-Path $root 'ProjectM.uproject') + '"'), '-run=pythonscript', ('-script="' + (Join-Path $PSScriptRoot ($Script + '.py')) + '"'), '-unattended', '-nop4', '-nosplash', '-stdout', '-FullStdOutLogOutput')
    $p = Start-Process -FilePath $ue -ArgumentList $list -RedirectStandardOutput $log -PassThru -NoNewWindow
    if (-not $p.WaitForExit(840000)) { Stop-Process -Id $p.Id -Force; "$Script TIMEOUT" }
    $hits = Select-String -Path $log -Pattern '[CCG]', 'LogPython: Error', 'Traceback', 'AssertionError', 'Error:' -SimpleMatch | Where-Object { $_.Line -notmatch 'LogStreaming|LogLinker|LogTexture' } | Select-Object -First 24
    foreach ($h in $hits) { $line = $h.Line; $i = $line.IndexOf('[CCG]'); if ($i -lt 0) { $i = [Math]::Max(0, $line.IndexOf('Log')) }; $line.Substring($i, [Math]::Min(300, $line.Length - $i)) }
    $ok = [bool](Select-String -Path $log -Pattern '[CCG] DONE' -SimpleMatch -Quiet)
    "$Script " + $(if ($ok) { 'OK' } else { 'FAILED' })
    return
}

foreach ($step in $Steps) {
    switch ($step) {
        'build' {
            $lines = & 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' ProjectMEditor Win64 Development ('-Project=' + (Join-Path $root 'ProjectM.uproject')) -WaitMutex -NoHotReloadFromIDE 2>&1
            $lines | Where-Object { $_ -match 'error|Result:|Total execution' } | Select-Object -Last 12
        }
        'stage' { Headless 'apply_stage' }
        'fx' {
            # Time-driven UI materials (stage ambience, button shine), made from nothing every time. Run before ui / select.
            $folder = Join-Path $root 'Content\Art\UI\CharacterCreate\Glass\Fx'
            foreach ($name in 'M_CCG_StageAmbience', 'M_CCG_Shine') {
                $file = Join-Path $folder ($name + '.uasset')
                if (Test-Path $file) { Remove-Item $file -ErrorAction Stop }
            }
            Headless 'apply_fx'
        }
        'dialog' {
            # Confirm dialogs (and their buttons), built from nothing every time. Run before 'select', which uses the red one.
            $folder = Join-Path $root 'Content\Art\UI\CharacterCreate\Glass\Dialog'
            foreach ($name in 'WBP_CCG_ConfirmDialog', 'WBP_CCG_ConfirmDialog_Danger', 'WBP_CCG_DialogPrimary', 'WBP_CCG_DialogDanger') {
                $file = Join-Path $folder ($name + '.uasset')
                if (Test-Path $file) { Remove-Item $file -ErrorAction Stop }
            }
            Headless 'apply_dialog'
        }
        'select' {
            # Built from nothing every time, like the creation widgets.
            $folder = Join-Path $root 'Content\Art\UI\CharacterCreate\Glass\Select'
            foreach ($name in 'WBP_CharacterSelect_Glass', 'WBP_CharacterSlot_Glass') {
                $file = Join-Path $folder ($name + '.uasset')
                if (Test-Path $file) { Remove-Item $file -ErrorAction Stop }
            }
            Headless 'apply_select'
        }
        'hair' {
            # The hair-tint materials are built from nothing every time (the field one is a fresh copy of the character material).
            $folder = Join-Path $root 'Content\Art\Materials\HairTint'
            foreach ($name in 'M_NUCharacterHairTint', 'M_NUHairTint_Stand', 'M_NUHairTint_UI') {      # _Stand: an earlier version made it, no longer built
                $file = Join-Path $folder ($name + '.uasset')
                if (Test-Path $file) { Remove-Item $file -ErrorAction Stop }
            }
            Headless 'apply_hair'
        }
        'ui' {
            # The three widgets are rebuilt from nothing every time: rebuilding inside an existing asset leaves stale variable ids (engine ensure).
            $glass = Join-Path $root 'Content\Art\UI\CharacterCreate\Glass'
            foreach ($name in 'WBP_CharacterCreate_Glass', 'WBP_CCG_SegButton', 'WBP_CCG_RaceChip', 'WBP_CCG_Swatch') {
                $file = Join-Path $glass ($name + '.uasset')
                if (Test-Path $file) { Remove-Item $file -ErrorAction Stop }      # fails while an editor has it open: close the editor first
            }
            Headless 'apply_ui'
        }
        'shot' {
            if ($States) {
                $file = (Resolve-Path $States).Path
                $names = (Get-Content $file -Raw -Encoding UTF8 | ConvertFrom-Json) | ForEach-Object { $_.shot }
                & (Join-Path $PSScriptRoot 'pie_capture.ps1') -Map $Map -Shot (Join-Path $out ($names[0] + '.png')) -Wait 14 -StepsFile $file -W $W -H $H
            } else {
                $names = @($Shot)
                & (Join-Path $PSScriptRoot 'pie_capture.ps1') -Map $Map -Shot (Join-Path $out ($Shot + '.png')) -Wait 14 -GamePy $GamePy -W $W -H $H
            }
            foreach ($name in $names) {
                $png = Join-Path $out ($name + '.png')
                if (Test-Path $png) { python (Join-Path $PSScriptRoot 'crop_shot.py') $png } else { "NO SHOT $name" }
            }
        }
    }
}
