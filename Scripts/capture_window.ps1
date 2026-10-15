param(
    [int]$TargetPid,
    [string]$OutPath = "C:\Users\Ndus_072\Documents\Unreal Projects\ProjectM\Saved\Screenshots\capture.png"
)

Add-Type -AssemblyName System.Drawing
. "$PSScriptRoot\game_window.ps1"

$hwnd = Find-GameWindow -TargetPid $TargetPid
if ($hwnd -eq [IntPtr]::Zero) { Write-Error "No game window for PID $TargetPid"; exit 1 }

[Win32Game]::ShowWindow($hwnd, 9) | Out-Null   # SW_RESTORE
[Win32Game]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 600

$rect = New-Object Win32Game+RECT
[Win32Game]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
$w = $rect.Right - $rect.Left
$h = $rect.Bottom - $rect.Top
if ($w -le 0 -or $h -le 0) { Write-Error "Bad rect $w x $h"; exit 1 }

$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bmp.Size)
$dir = Split-Path $OutPath
if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force $dir | Out-Null }
$bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output "Saved $OutPath ($w x $h)"
