# Waits until no ProjectM editor / commandlet is running (other sessions share this project), then runs the given
# commands in order. Checks twice a second and needs two clear checks in a row.  .\wait_free.ps1 -Then "build","apply"
param([string[]]$Then = @(), [int]$TimeoutMin = 90)
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$deadline = (Get-Date).AddMinutes($TimeoutMin)
$clear = 0
while ((Get-Date) -lt $deadline) {
    $busy = @(Get-CimInstance Win32_Process -Filter "Name like 'UnrealEditor%'" | Where-Object { $_.CommandLine -like '*RevStudio-ProjectM*' })
    if ($busy.Count -eq 0) { $clear++ } else { $clear = 0 }
    if ($clear -ge 2) { break }
    Start-Sleep -Milliseconds 500
}
if ($clear -lt 2) { 'TIMEOUT waiting for a free editor slot'; return }
'free at ' + (Get-Date -Format HH:mm:ss)
foreach ($step in $Then) {
    switch ($step) {
        'build' { & (Join-Path $root 'Tools\CharacterCreateGlass\run.ps1') -Steps build 2>&1 | Select-Object -Last 6 }
        default { & (Join-Path $PSScriptRoot 'run.ps1') -Steps $step }
    }
}
