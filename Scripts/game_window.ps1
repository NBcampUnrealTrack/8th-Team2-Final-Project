# Shared: find the actual game window of a PID by title (the -log console also
# belongs to the PID and sometimes becomes MainWindowHandle).
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class Win32Game {
    public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr lParam);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint uCode, uint uMapType);
    [DllImport("user32.dll")] public static extern int GetClassName(IntPtr hWnd, StringBuilder text, int count);
    public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

function Find-GameWindow([int]$TargetPid, [string]$TitleMatch = "ProjectM (") {
    $found = [IntPtr]::Zero
    $cb = [Win32Game+EnumProc]{
        param($hWnd, $lParam)
        $winPid = 0
        [Win32Game]::GetWindowThreadProcessId($hWnd, [ref]$winPid) | Out-Null
        if ($winPid -eq $TargetPid -and [Win32Game]::IsWindowVisible($hWnd)) {
            $sb = New-Object System.Text.StringBuilder 256
            [Win32Game]::GetWindowText($hWnd, $sb, 256) | Out-Null
            $cls = New-Object System.Text.StringBuilder 256
            [Win32Game]::GetClassName($hWnd, $cls, 256) | Out-Null
            # -log console shares PID and a similar title; only the real game
            # viewport uses window class "UnrealWindow".
            if ($sb.ToString().Contains($TitleMatch) -and $cls.ToString() -eq 'UnrealWindow') {
                Set-Variable -Name found -Value $hWnd -Scope 1
                return $false
            }
        }
        return $true
    }
    [Win32Game]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
    return $found
}
