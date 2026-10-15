param(
    [int]$TargetPid,
    [string]$Key,          # single char like 'W' or names: SPACE, ENTER, UP, DOWN, LEFT, RIGHT, Q
    [int]$HoldMs = 80
)

. "$PSScriptRoot\game_window.ps1"

$vkMap = @{
    'SPACE' = 0x20; 'ENTER' = 0x0D; 'ESC' = 0x1B;
    'UP' = 0x26; 'DOWN' = 0x28; 'LEFT' = 0x25; 'RIGHT' = 0x27
}
$k = $Key.ToUpper()
if ($vkMap.ContainsKey($k)) { $vk = [byte]$vkMap[$k] }
else { $vk = [byte][char]$k }

$hwnd = Find-GameWindow -TargetPid $TargetPid
if ($hwnd -eq [IntPtr]::Zero) { Write-Error "No game window for PID $TargetPid"; exit 1 }
[Win32Game]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 150

$scan = [byte]([Win32Game]::MapVirtualKey($vk, 0))
[Win32Game]::keybd_event($vk, $scan, 0, [UIntPtr]::Zero)          # key down
Start-Sleep -Milliseconds $HoldMs
[Win32Game]::keybd_event($vk, $scan, 2, [UIntPtr]::Zero)          # key up (KEYEVENTF_KEYUP)
Write-Output "Sent $Key (vk=$vk) hold=${HoldMs}ms"
