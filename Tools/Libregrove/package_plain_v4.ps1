param([switch]$RequireComplete)
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../Docs/Libregrove/PlainModel_References_V4_20260908'))
$taskRecords = Get-Content -LiteralPath (Join-Path $taskRoot 'generation_records.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$taskCatalog = Get-Content -LiteralPath (Join-Path $taskRoot 'catalog.json') -Raw -Encoding UTF8 | ConvertFrom-Json
Add-Type -AssemblyName System.Drawing
$taskManifest = @()
$taskEvidence = @()
foreach ($taskRecord in $taskRecords) {
    if ($taskRecord.visual_qa -eq 'pending') { throw "Unreviewed record: $($taskRecord.id)/$($taskRecord.view)" }
    $taskSource = [IO.Path]::GetFullPath($taskRecord.source)
    if (-not (Test-Path -LiteralPath $taskSource -PathType Leaf)) { throw "Missing source $taskSource" }
    if ((Get-Item -LiteralPath $taskSource).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Source must not be a symlink' }
    $taskAccepted = $taskRecord.visual_qa.StartsWith('accepted')
    if ($taskAccepted) {
        $taskSubdir = if ($taskRecord.view -eq 'front_textured') { 'Appearance' } else { 'Geometry' }
        $taskRelative = "Models/$($taskRecord.id)/$taskSubdir/$($taskRecord.view).png"
    } else {
        $taskRelative = "Rejected/$($taskRecord.id)_$($taskRecord.view)_attempt$($taskRecord.attempt).png"
    }
    $taskDestination = [IO.Path]::GetFullPath((Join-Path $taskRoot $taskRelative))
    if (-not $taskDestination.StartsWith($taskRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Destination escaped pack' }
    $taskSourceHash = (Get-FileHash -LiteralPath $taskSource -Algorithm SHA256).Hash.ToLowerInvariant()
    if (Test-Path -LiteralPath $taskDestination) {
        if ((Get-FileHash -LiteralPath $taskDestination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskSourceHash) { throw "Will not overwrite different artifact: $taskDestination" }
    } else {
        New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($taskDestination)) -Force | Out-Null
        Copy-Item -LiteralPath $taskSource -Destination $taskDestination
    }
    $taskBitmap = [Drawing.Bitmap]::FromFile($taskDestination)
    try {
        $taskWidth = $taskBitmap.Width; $taskHeight = $taskBitmap.Height
        if ($taskWidth -ne $taskHeight -or $taskWidth -lt 1024) { throw "Bad square reference size: $taskRelative" }
        if ($taskBitmap.RawFormat.Guid -ne [Drawing.Imaging.ImageFormat]::Png.Guid) { throw "Not PNG: $taskRelative" }
        $taskMin = 255; $taskMax = 0; $taskForeground = 0; $taskSamples = 0
        for ($taskY = 0; $taskY -lt $taskHeight; $taskY += 11) {
            for ($taskX = 0; $taskX -lt $taskWidth; $taskX += 11) {
                $taskPixel = $taskBitmap.GetPixel($taskX,$taskY)
                $taskLum = ($taskPixel.R + $taskPixel.G + $taskPixel.B) / 3
                $taskMin = [Math]::Min($taskMin,$taskLum); $taskMax = [Math]::Max($taskMax,$taskLum)
                if ($taskLum -lt 235) { $taskForeground++ }; $taskSamples++
            }
        }
        if (($taskMax - $taskMin) -lt 25 -or $taskForeground -lt 100) { throw "Empty or low-content image: $taskRelative" }
    } finally { $taskBitmap.Dispose() }
    $taskFinalHash = (Get-FileHash -LiteralPath $taskDestination -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($taskFinalHash -ne $taskSourceHash) { throw 'Copy hash mismatch' }
    $taskItem = [ordered]@{ id=$taskRecord.id; view=$taskRecord.view; attempt=$taskRecord.attempt; accepted=$taskAccepted; file=$taskRelative; width=$taskWidth; height=$taskHeight; format='PNG'; bytes=(Get-Item -LiteralPath $taskDestination).Length; sha256=$taskFinalHash; source_sha256=$taskSourceHash; decoded=$true; foreground_sample_ratio=[Math]::Round($taskForeground/$taskSamples,4); visual_qa=$taskRecord.visual_qa; native_tool='image_gen.imagegen'; tool_artifact_id=[IO.Path]::GetFileNameWithoutExtension($taskSource) }
    $taskEvidence += $taskItem
    if ($taskAccepted) { $taskManifest += $taskItem }
}
if (($taskManifest | Group-Object id,view | Where-Object Count -gt 1).Count -gt 0) { throw 'Duplicate accepted views' }
if ($RequireComplete) {
    if ($taskManifest.Count -ne 36) { throw "Expected36 accepted images, got $($taskManifest.Count)" }
    foreach ($taskAsset in $taskCatalog) { foreach ($taskView in @('front','right','back','front_textured')) {
        if (-not ($taskManifest | Where-Object { $_.id -eq $taskAsset.id -and $_.view -eq $taskView })) { throw "Missing $($taskAsset.id)/$taskView" }
    }}
    if (($taskManifest.sha256 | Sort-Object -Unique).Count -ne 36) { throw 'Final images must be independently generated, distinct files' }
}
$taskReport = [ordered]@{ version='PlainModel_V4'; date='2026-09-08'; scope='AI concept/reference images, not actual meshes or texture maps'; effective_agent='Current Codex session; exact agent slug not exposed'; image_model='Not exposed by native tool'; provenance='Direct native tool calls, system-returned artifact paths; source and copied-output SHA256 verified'; accepted_count=$taskManifest.Count; rejected_count=($taskEvidence.Count-$taskManifest.Count); pixel_identical_multiview_alignment=$false; actual_mesh_vertex_counts_measured=$false; imported_to_unreal=$false; files=$taskManifest; all_attempts=$taskEvidence }
$taskReport | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $taskRoot 'manifest.json') -Encoding UTF8
$taskCatalog | ForEach-Object { [pscustomobject]@{ id=$_.id; name=$_.name; vertex_min=$_.verts[0]; vertex_max=$_.verts[1]; triangle_review_max=$_.tris; design_width_m=$_.design_m[0]; design_depth_m=$_.design_m[1]; design_height_m=$_.design_m[2]; game_width_cm=$_.game_cm[0]; game_depth_cm=$_.game_cm[1]; game_height_cm=$_.game_cm[2]; basis='Hand-cleaned future mesh target, NOT measured counts' } } | Export-Csv -LiteralPath (Join-Path $taskRoot 'budgets.csv') -NoTypeInformation -Encoding UTF8
[pscustomobject]@{ accepted=$taskManifest.Count; rejected=($taskEvidence.Count-$taskManifest.Count); root=$taskRoot; bytes=($taskEvidence | Measure-Object bytes -Sum).Sum } | ConvertTo-Json
