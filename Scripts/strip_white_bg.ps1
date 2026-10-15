# Flood-fills near-white pixels connected to the image border with transparency.
# Interior white pixels (eyes, highlights) survive because they are enclosed.
param(
    [string]$Dir,
    [int]$Tolerance = 16    # per-channel distance from pure white
)

Add-Type -AssemblyName System.Drawing

$files = Get-ChildItem $Dir -Filter *.png
foreach ($file in $files) {
    $src = [System.Drawing.Bitmap]::FromFile($file.FullName)
    $bmp = New-Object System.Drawing.Bitmap -ArgumentList $src.Width, $src.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.DrawImageUnscaled($src, 0, 0)
    $g.Dispose(); $src.Dispose()

    $w = $bmp.Width; $h = $bmp.Height
    $visited = New-Object 'bool[,]' $w, $h
    $queue = New-Object System.Collections.Generic.Queue[int[]]

    $isWhite = {
        param($p)
        ($p.A -gt 0) -and (255 - $p.R) -le $Tolerance -and (255 - $p.G) -le $Tolerance -and (255 - $p.B) -le $Tolerance
    }

    for ($x = 0; $x -lt $w; $x++) {
        foreach ($y in @(0, ($h - 1))) {
            if (-not $visited[$x, $y] -and (& $isWhite $bmp.GetPixel($x, $y))) { $queue.Enqueue(@($x, $y)); $visited[$x, $y] = $true }
        }
    }
    for ($y = 0; $y -lt $h; $y++) {
        foreach ($x in @(0, ($w - 1))) {
            if (-not $visited[$x, $y] -and (& $isWhite $bmp.GetPixel($x, $y))) { $queue.Enqueue(@($x, $y)); $visited[$x, $y] = $true }
        }
    }

    $cleared = 0
    while ($queue.Count -gt 0) {
        $pt = $queue.Dequeue()
        $x = $pt[0]; $y = $pt[1]
        $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
        $cleared++
        foreach ($d in @(@(1,0), @(-1,0), @(0,1), @(0,-1))) {
            $nx = $x + $d[0]; $ny = $y + $d[1]
            if ($nx -ge 0 -and $nx -lt $w -and $ny -ge 0 -and $ny -lt $h -and -not $visited[$nx, $ny]) {
                if (& $isWhite $bmp.GetPixel($nx, $ny)) { $queue.Enqueue(@($nx, $ny)); $visited[$nx, $ny] = $true }
            }
        }
    }

    $bmp.Save($file.FullName, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output "$($file.Name): cleared $cleared px"
}
