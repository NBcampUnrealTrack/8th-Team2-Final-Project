# Autoplay verification: strike timing (offense action command) + parry.
# Launches the game, walks into the chasing slime, attacks with Space spam
# (input buffer keeps the press closest to impact -> PERFECT), then parries.
$ErrorActionPreference = 'Stop'

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Input2 {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
    public const uint KEYUP = 0x0002;
    public static void Down(byte vk) { keybd_event(vk, 0, 0, UIntPtr.Zero); }
    public static void Up(byte vk)   { keybd_event(vk, 0, KEYUP, UIntPtr.Zero); }
    public static void Tap(byte vk)  { Down(vk); System.Threading.Thread.Sleep(35); Up(vk); }
}
"@

. "$PSScriptRoot\game_window.ps1"

$VK_W = 0x57; $VK_D = 0x44; $VK_F = 0x46; $VK_SPACE = 0x20

$Proj = 'C:\Users\Ndus_072\Documents\Unreal Projects\ProjectM'
# ProjectM.exe (game target) needs cooked content -> use editor binary with -game
$Exe  = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$LogDir = Join-Path $Proj 'Saved\Logs'

$P = Start-Process -FilePath $Exe -ArgumentList "`"$Proj\ProjectM.uproject`"", '-game', '-log', '-ForceLogFlush', '-windowed', '-resx=1280', '-resy=720' -PassThru
Write-Output "PID: $($P.Id)"

# wait for the actual game viewport window (NOT the -log console)
$hwnd = [IntPtr]::Zero
$deadline = 90
while ($deadline -gt 0) {
    Start-Sleep -Seconds 2; $deadline -= 2
    $P.Refresh()
    if ($P.HasExited) { Write-Output 'GAME EXITED EARLY'; exit 1 }
    $hwnd = Find-GameWindow -TargetPid $P.Id
    if ($hwnd -ne [IntPtr]::Zero) { break }
}
if ($hwnd -eq [IntPtr]::Zero) { Write-Output 'NO GAME WINDOW'; Stop-Process -Id $P.Id -Force; exit 1 }
Start-Sleep -Seconds 8   # settle: BeginPlay, scene spawn
[Win32Game]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 800

# --- walk toward slime #1 at (900, 700): W = +X, D = +Y ---
[Input2]::Down($VK_W); [Input2]::Down($VK_D)
Start-Sleep -Milliseconds 3200
[Input2]::Up($VK_W); [Input2]::Up($VK_D)
Write-Output 'walked; waiting for chase contact'
& "$PSScriptRoot\capture_window.ps1" -TargetPid $P.Id -OutPath "$Proj\Saved\Screenshots\verify_field.png"
Start-Sleep -Seconds 4    # chase + contact + camera blend + turn calc
& "$PSScriptRoot\capture_window.ps1" -TargetPid $P.Id -OutPath "$Proj\Saved\Screenshots\verify_battle_menu.png"

function Spam-Space([int]$Count, [int]$GapMs) {
    for ($i = 0; $i -lt $Count; $i++) { [Input2]::Tap($VK_SPACE); Start-Sleep -Milliseconds $GapMs }
}

# --- player turn 1: ATTACK -> target -> strike-timing spam ---
[Input2]::Tap($VK_F); Start-Sleep -Milliseconds 500   # ATTACK
[Input2]::Tap($VK_F); Start-Sleep -Milliseconds 200   # confirm target
Spam-Space 40 30                                       # covers 0.9s window
Write-Output 'turn1 done'
Start-Sleep -Seconds 2

# --- enemy turn: parry spam (telegraph 1.2s) ---
Spam-Space 40 30
& "$PSScriptRoot\capture_window.ps1" -TargetPid $P.Id -OutPath "$Proj\Saved\Screenshots\verify_enemy_turn.png"
Spam-Space 40 30
Write-Output 'parry spam done'
Start-Sleep -Seconds 2

# --- player turn 2 ---
[Input2]::Tap($VK_F); Start-Sleep -Milliseconds 500
[Input2]::Tap($VK_F); Start-Sleep -Milliseconds 200
Spam-Space 40 30
Write-Output 'turn2 done'
Start-Sleep -Seconds 6

# graceful close first so the log buffer flushes
$P.CloseMainWindow() | Out-Null
if (-not $P.WaitForExit(10000)) { Stop-Process -Id $P.Id -Force -ErrorAction SilentlyContinue }
Start-Sleep -Seconds 2

# --- newest log written by this run ---
$Log = Get-ChildItem $LogDir -Filter 'ProjectM*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
Write-Output "LOG: $($Log.FullName)"
Get-Content $Log.FullName | Select-String -Pattern '\[Ember\]' | ForEach-Object { $_.Line }
