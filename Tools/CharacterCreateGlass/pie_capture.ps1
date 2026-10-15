# Launches an editor instance, captures a map in play mode with UI, quits. PowerShell so /Game/... paths are not rewritten.
param([string]$Map = '/Game/Maps/L_CharacterCreate', [string]$Shot, [int]$Wait = 12, [int]$W = 1920, [int]$H = 1078, [string]$GamePy = '', [string]$StepsFile = '')
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$env:CC_SHOT = $Shot; $env:CC_WAIT = "$Wait"; $env:CC_W = "$W"; $env:CC_H = "$H"; $env:CC_GAME_PY = $GamePy; $env:CC_STEPS = $StepsFile
$log = [IO.Path]::ChangeExtension($Shot, '.log')
$py = (Join-Path $PSScriptRoot 'pie_capture.py').Replace([char]92, [char]47)
$list = @(('"' + (Join-Path $root 'ProjectM.uproject') + '"'), $Map, ('-ExecCmds="py ' + $py + '"'), '-unattended', '-nosplash', '-NoSound', '-windowed', '-resx=1280', '-resy=720', '-log', ('-abslog="' + $log + '"'))
$snapshot = [IO.Path]::ChangeExtension($Shot, '.playsettings.json')
python (Join-Path $PSScriptRoot 'play_settings.py') save $snapshot | Out-Null
$p = Start-Process -FilePath $ue -ArgumentList $list -PassThru
if (-not $p.WaitForExit(420000)) { Stop-Process -Id $p.Id -Force; 'TIMEOUT killed' }
python (Join-Path $PSScriptRoot 'play_settings.py') restore $snapshot   # play start writes the capture window size into the user's ini
Select-String -Path $log -Pattern '[CCG]', 'LogPython: Error' -SimpleMatch | Select-Object -First 40 | ForEach-Object { $i = $_.Line.IndexOf('[CCG]'); if ($i -lt 0) { $i = 0 }; $_.Line.Substring($i) }
if (-not $StepsFile) { if (Test-Path $Shot) { 'OK ' + $Shot } else { 'NO SHOT' } }
