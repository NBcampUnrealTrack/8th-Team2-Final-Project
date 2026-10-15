param([Parameter(Mandatory=$true)][string]$SourcePng)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$outputDir = Join-Path $projectRoot 'ArtSource/UI/QuestOffer'
if ((Get-FileHash -LiteralPath $SourcePng -Algorithm MD5).Hash -ne '1D22556733ABB67127B4135769D86CB9') {
    throw 'Source does not match the imported quest-offer reference.'
}
[IO.Directory]::CreateDirectory($outputDir) | Out-Null
$sourceImage = [Drawing.Bitmap]::new([IO.Path]::GetFullPath($SourcePng))
try {
    if ($sourceImage.Width -ne 1742 -or $sourceImage.Height -ne 903) { throw 'Unexpected source dimensions' }
    $regions = @(
        @{ Name='T_QuestOffer_Compass'; X=484; Y=186; W=117; H=117 },
        @{ Name='T_QuestOffer_City'; X=446; Y=422; W=198; H=214 }
    )
    foreach ($region in $regions) {
        # Preserve two source pixels around each edge for identical bilinear sampling.
        $rect = [Drawing.Rectangle]::new($region.X-2, $region.Y-2, $region.W+4, $region.H+4)
        $cropped = $sourceImage.Clone($rect, $sourceImage.PixelFormat)
        try {
            $outputPath = Join-Path $outputDir ($region.Name + '.png')
            if (Test-Path -LiteralPath $outputPath) { throw "Output already exists: $outputPath" }
            $cropped.Save($outputPath, [Drawing.Imaging.ImageFormat]::Png)
            $verified = [Drawing.Bitmap]::new($outputPath)
            try {
                for ($y=0; $y -lt $rect.Height; $y++) {
                    for ($x=0; $x -lt $rect.Width; $x++) {
                        if ($verified.GetPixel($x,$y).ToArgb() -ne $sourceImage.GetPixel($rect.X+$x,$rect.Y+$y).ToArgb()) {
                            throw "Pixel mismatch: $outputPath at $x,$y"
                        }
                    }
                }
            } finally { $verified.Dispose() }
            Write-Output "$outputPath : $($rect.Width)x$($rect.Height), every pixel verified"
        } finally { $cropped.Dispose() }
    }
} finally { $sourceImage.Dispose() }
