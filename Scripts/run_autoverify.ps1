# Self-playing verification: ember.AutoVerify 1 drives walk->battle->timing.
# No external input injection needed; we only watch and screenshot.
$ErrorActionPreference = 'Stop'

$Proj = 'C:\Users\Ndus_072\Documents\Unreal Projects\ProjectM'
$Exe  = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$Shots = Join-Path $Proj 'Saved\Screenshots'

$P = Start-Process -FilePath $Exe -ArgumentList "`"$Proj\ProjectM.uproject`"", '-game', '-log', '-ForceLogFlush', '-windowed', '-resx=1280', '-resy=720', '-ExecCmds="ember.AutoVerify 1"' -PassThru
Write-Output "PID: $($P.Id)"

# engine boot ~35s, then the pawn starts walking on its own
Start-Sleep -Seconds 45
& "$PSScriptRoot\capture_window.ps1" -TargetPid $P.Id -OutPath "$Shots\auto_1.png"
Start-Sleep -Seconds 10
& "$PSScriptRoot\capture_window.ps1" -TargetPid $P.Id -OutPath "$Shots\auto_2.png"
Start-Sleep -Seconds 10
& "$PSScriptRoot\capture_window.ps1" -TargetPid $P.Id -OutPath "$Shots\auto_3.png"
Start-Sleep -Seconds 10
& "$PSScriptRoot\capture_window.ps1" -TargetPid $P.Id -OutPath "$Shots\auto_4.png"
Start-Sleep -Seconds 15
& "$PSScriptRoot\capture_window.ps1" -TargetPid $P.Id -OutPath "$Shots\auto_5.png"
Start-Sleep -Seconds 15
& "$PSScriptRoot\capture_window.ps1" -TargetPid $P.Id -OutPath "$Shots\auto_6.png"

$P.Refresh()
if (-not $P.HasExited) {
    $P.CloseMainWindow() | Out-Null
    if (-not $P.WaitForExit(10000)) { Stop-Process -Id $P.Id -Force -ErrorAction SilentlyContinue }
}
Start-Sleep -Seconds 2

$Log = Get-ChildItem "$Proj\Saved\Logs" -Filter 'ProjectM*.log' |
    Where-Object { $_.Name -notlike '*backup*' } |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
Write-Output "LOG: $($Log.FullName)"
Get-Content $Log.FullName | Select-String -Pattern '\[Ember\]' | ForEach-Object { $_.Line }
