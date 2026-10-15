$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../Docs/Libregrove/DirectModel_Cottage_V4_20260908'))
$taskExchange = 'C:/Users/Ndus_072/AppData/Local/Revvy/BlenderExchange/Libregrove_DirectCottage_V4_20260908'
$taskFiles = @{
  'SM_LG_Cottage_Plain.blend'='SM_LG_Cottage_Plain.blend'
  'SM_LG_Cottage_Plain.glb'='SM_LG_Cottage_Plain.glb'
  'SM_LG_Cottage_Plain.fbx'='SM_LG_Cottage_Plain.fbx'
  '01_live_model.png'='Preview.png'
  '00_TextureCover_before.blend'='Backup/00_TextureCover_before.blend'
}
$taskHashRecords = @()
foreach ($taskSourceName in $taskFiles.Keys) {
  $taskSource = Join-Path $taskExchange $taskSourceName
  $taskTarget = Join-Path $taskRoot $taskFiles[$taskSourceName]
  New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($taskTarget)) -Force | Out-Null
  if (-not (Test-Path -LiteralPath $taskTarget)) { Copy-Item -LiteralPath $taskSource -Destination $taskTarget }
  $taskSourceHash = (Get-FileHash -LiteralPath $taskSource -Algorithm SHA256).Hash
  $taskTargetHash = (Get-FileHash -LiteralPath $taskTarget -Algorithm SHA256).Hash
  if ($taskSourceHash -ne $taskTargetHash) { throw 'Copy hash mismatch' }
  $taskHashRecords += [pscustomobject]@{ file=$taskFiles[$taskSourceName]; sha256=$taskTargetHash.ToLowerInvariant(); bytes=(Get-Item -LiteralPath $taskTarget).Length }
}
$taskAuditText = & 'C:/Users/Ndus_072/AppData/Local/Programs/Python/Python312/python.exe' 'Tools/Libregrove/inspect_glb.py' (Join-Path $taskRoot 'SM_LG_Cottage_Plain.glb')
if ($LASTEXITCODE -ne 0) { throw 'GLB audit failed' }
$taskAudit = ($taskAuditText -join "`n") | ConvertFrom-Json
if ($taskAudit.total_triangles -ne 56 -or $taskAudit.primitives.Count -ne 5) { throw 'Unexpected exported geometry' }
$taskExpected = @(2.25,2.43,2.7)
for ($taskAxis=0; $taskAxis -lt 3; $taskAxis++) {
  if ([Math]::Abs($taskAudit.dimensions_m_xyz_y_up[$taskAxis]-$taskExpected[$taskAxis]) -gt 0.00001) { throw 'GLB dimensions mismatch' }
}
$taskMeasure = (Get-Content -Raw -Encoding utf8 (Join-Path $taskRoot 'evidence/measure_assert_002.json') | ConvertFrom-Json)[-1].structuredContent.result
if (-not $taskMeasure.passed -or $taskMeasure.assertions.Count -ne 17) { throw 'Measurement assertions failed' }
$taskOriginal = (Get-FileHash -LiteralPath 'D:/Users/Ndus_072/Desktop/TextureCover.blend' -Algorithm SHA256).Hash.ToLowerInvariant()
if ($taskOriginal -ne 'e4f4a1c80027142c34c9a6c870fb7520c96d87f2109eb5d052cbf053a0f9dac9') { throw 'Original file changed' }
$taskReports = @()
for ($taskIndex=1; $taskIndex -le 5; $taskIndex++) {
  $taskHistory = Get-Content -Raw -Encoding utf8 (Join-Path $taskRoot ('evidence/build_{0:00}_inspect.json' -f $taskIndex)) | ConvertFrom-Json
  $taskReport = $taskHistory[-1].structuredContent.result
  if (-not $taskReport.quality.passed) { throw 'Part geometry QA failed' }
  $taskReports += [pscustomobject]@{name=$taskReport.object.name;fingerprint=$taskReport.fingerprint;geometry=$taskReport.geometry;uv=$taskReport.uv_layers;warnings=$taskReport.quality.warnings}
}
$taskVerification = [ordered]@{ status='PROTOTYPE_GEOMETRY_COMPLETE_NOT_TEXTURE_READY'; blender='5.0.0'; bridge='1.2.0'; compatibility=$true; mesh_vertices=38; triangles=56; polygons=29; object_count=5; dimensions_cm_width_depth_height=@(270,225,243); numeric_assertions_passed=17; source_preserved=$true; original_sha256=$taskOriginal; texture_maps_created=$false; unified_uv_atlas=$false; globally_welded=$false; applied_object_scale=$false; closeup_ui_action_verified=$false; parts=$taskReports; glb_audit=$taskAudit; files=$taskHashRecords }
$taskVerification | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath (Join-Path $taskRoot 'verification.json') -Encoding UTF8
$taskZip = [IO.Path]::GetFullPath((Join-Path $taskRoot '../../Libregrove_DirectCottage_V4_20260908.zip'))
if (Test-Path -LiteralPath $taskZip) { throw 'Zip already exists' }
Compress-Archive -LiteralPath $taskRoot -DestinationPath $taskZip -CompressionLevel Optimal
[pscustomobject]@{ folder=$taskRoot; zip=$taskZip; glb_render_vertices=$taskAudit.total_render_vertices; triangles=$taskAudit.total_triangles; dimensions_m_y_up=$taskAudit.dimensions_m_xyz_y_up; zip_sha256=(Get-FileHash -LiteralPath $taskZip -Algorithm SHA256).Hash.ToLowerInvariant() } | ConvertTo-Json
