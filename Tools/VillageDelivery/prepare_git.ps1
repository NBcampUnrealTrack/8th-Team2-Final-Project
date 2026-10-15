param([switch]$Stage)
$ErrorActionPreference = 'Stop'
$deliveryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Set-Location -LiteralPath $deliveryRoot
$selected = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$excludedSensitive = [Collections.Generic.List[string]]::new()
$knownSecrets = @('REVVY_FULL_ACCESS_TOKEN', 'TRIPO_API_KEY', 'TRIPO3D_API_KEY') | ForEach-Object {
    foreach ($target in 'Process','User') {
        $value = [Environment]::GetEnvironmentVariable($_, $target)
        if ($value -and $value.Length -ge 16) { $value }
    }
}
$secretPatterns = @(
    '(?i)\b(?:ghp_|github_pat_|sk-|tsk_)[A-Za-z0-9_-]{20,}',
    '(?i)\b(?:Bearer|Basic)\s+[A-Za-z0-9+/_.=-]{24,}',
    '(?i)[?&](?:X-Amz-Signature|X-Goog-Signature|signature|sig|access_token|token)=',
    '-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----',
    '\beyJ[A-Za-z0-9_-]{16,}\.[A-Za-z0-9_-]{16,}\.[A-Za-z0-9_-]{16,}'
)
function Add-DeliveryFile([string]$path) {
    $file = Get-Item -LiteralPath $path
    $relative = $file.FullName.Substring($deliveryRoot.Length + 1).Replace('\','/')
    if ($relative -match '(^|/)(__pycache__|evidence)(/|$)' -or $file.Extension -in '.pyc','.log','.rsp','.zip','.blend1') { return }
    if ($file.Extension -in '.py','.ps1','.json','.md','.txt','.hlsl','.svg','.csv' -or $file.Name -eq '.gitignore') {
        $body = [IO.File]::ReadAllText($file.FullName)
        $hasSecret = $false
        foreach ($secret in $knownSecrets) { if ($body.Contains($secret)) { $hasSecret = $true; break } }
        foreach ($pattern in $secretPatterns) { if ($body -match $pattern) { $hasSecret = $true; break } }
        if ($hasSecret) { $excludedSensitive.Add($relative); return }
    }
    [void]$selected.Add($relative)
}
foreach ($folder in 'Content/Cindehearth','Content/Libregrove') {
    Get-ChildItem -LiteralPath $folder -File -Recurse | ForEach-Object { Add-DeliveryFile $_.FullName }
}
Add-DeliveryFile 'Content/Maps/L_Cindehearth.umap'
foreach ($folder in 'Tools/Cindehearth','Tools/Libregrove','Tools/VillageDelivery') {
    Get-ChildItem -LiteralPath $folder -File -Recurse | Where-Object {
        $_.Extension -in '.py','.ps1','.json','.md','.hlsl','.svg','.csv' -or $_.Name -eq '.gitignore'
    } | ForEach-Object { Add-DeliveryFile $_.FullName }
}
foreach ($folder in 'Docs/Libregrove/Level_V8_20260908','Docs/Libregrove/Unreal_Textured_V7_20260908','Docs/Villages') {
    Get-ChildItem -LiteralPath $folder -File -Recurse | Where-Object {
        $_.Extension -in '.md','.json','.csv' -and $_.Name -notmatch 'schema|blender_scene|source_meshes'
    } | ForEach-Object { Add-DeliveryFile $_.FullName }
}
# Keep linked review frames (and directory-gallery contents) next to their guides.
foreach ($pass in 1..3) {
    foreach ($path in @($selected | Where-Object { $_.EndsWith('.md') })) {
        $body = [IO.File]::ReadAllText((Join-Path $deliveryRoot $path))
        foreach ($link in [regex]::Matches($body,'\]\(([^)#]+)(?:#[^)]*)?\)')) {
            $target = $link.Groups[1].Value
            if ($target -match '^[A-Za-z]+:' -or $target.StartsWith('/')) { continue }
            $resolved = [IO.Path]::GetFullPath((Join-Path (Split-Path (Join-Path $deliveryRoot $path)) $target))
            if (-not $resolved.StartsWith($deliveryRoot + [IO.Path]::DirectorySeparatorChar)) { continue }
            if (Test-Path -LiteralPath $resolved -PathType Leaf) { Add-DeliveryFile $resolved }
            elseif (Test-Path -LiteralPath $resolved -PathType Container) {
                Get-ChildItem -LiteralPath $resolved -File | Where-Object Extension -in '.png','.jpg','.md' | ForEach-Object { Add-DeliveryFile $_.FullName }
            }
        }
    }
}
$files = @($selected | Sort-Object)
$bytes = ($files | ForEach-Object { (Get-Item -LiteralPath $_).Length } | Measure-Object -Sum).Sum
Write-Output ('Selected: {0} files, {1:N1} MiB' -f $files.Count,($bytes/1MB))
Write-Output ('Excluded sensitive/signed-response files: '+$excludedSensitive.Count)
$excludedSensitive | Sort-Object -Unique | Select-Object -First 12 | ForEach-Object { Write-Output ('Excluded path only: '+$_) }
$files | Group-Object { ($_ -split '/')[0..1] -join '/' } | Select-Object Count,Name | Format-Table -AutoSize
if ($Stage) {
    if (@(git diff --cached --name-only).Count) { throw 'Index is not empty. Preserve existing staged work; inspect before staging.' }
    for ($i=0; $i -lt $files.Count; $i+=60) {
        $last = [Math]::Min($i+59,$files.Count-1)
        & git add -- @($files[$i..$last])
        if ($LASTEXITCODE -ne 0) { throw 'git add failed; inspect existing stage, do not retry blindly.' }
    }
    Write-Output 'Scoped staging complete. No commit or push performed by this script.'
}
