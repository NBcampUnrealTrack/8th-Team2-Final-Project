param(
  [Parameter(Mandatory=$true)][string]$Tool,
  [string]$ArgsJson = '{}',
  [string]$ArgsFile = '',
  [int]$TimeoutSec = 600
)
$tok = [Environment]::GetEnvironmentVariable('REVVY_FULL_ACCESS_TOKEN','User')
$url = 'http://127.0.0.1:8091/mcp'
if ($ArgsFile) { $ArgsJson = [IO.File]::ReadAllText($ArgsFile, [Text.Encoding]::UTF8) }
$argsObj = $ArgsJson | ConvertFrom-Json
$req = @{ jsonrpc='2.0'; id=1; method='tools/call'; params=@{ name=$Tool; arguments=$argsObj } } | ConvertTo-Json -Depth 50 -Compress
$hdr = @{ Authorization = "Bearer $tok"; Accept = 'application/json, text/event-stream' }
try {
  $r = Invoke-WebRequest -Uri $url -Method Post -Body ([Text.Encoding]::UTF8.GetBytes($req)) -ContentType 'application/json; charset=utf-8' -Headers $hdr -UseBasicParsing -TimeoutSec $TimeoutSec
} catch {
  Write-Output "HTTP ERROR: $($_.Exception.Message)"
  if ($_.Exception.Response) { $sr = New-Object IO.StreamReader($_.Exception.Response.GetResponseStream()); Write-Output $sr.ReadToEnd() }
  exit 1
}
$txt = [Text.Encoding]::UTF8.GetString($r.RawContentStream.ToArray())
if ($txt -match '^event:|^data:') { $txt = ($txt -split "`n" | Where-Object { $_ -like 'data:*' } | ForEach-Object { $_.Substring(5).Trim() }) -join "`n" }
try { $o = $txt | ConvertFrom-Json } catch { Write-Output $txt; exit 0 }
if ($o.error) { Write-Output ("RPC ERROR: " + ($o.error | ConvertTo-Json -Depth 10 -Compress)); exit 1 }
$res = $o.result
if ($res.content) { foreach ($c in $res.content) { if ($c.type -eq 'text') { Write-Output $c.text } else { Write-Output ($c | ConvertTo-Json -Depth 10 -Compress) } } }
elseif ($res) { Write-Output ($res | ConvertTo-Json -Depth 20) }
if ($res.isError) { exit 1 }
