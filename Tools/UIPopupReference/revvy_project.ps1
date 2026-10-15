param(
    [string]$Method = 'tools/list',
    [string]$Tool,
    [string]$ArgumentsFile,
    [switch]$Full
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$config = Get-Content -LiteralPath (Join-Path $projectRoot '.mcp.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$env:REVVY_MCP_URL = $config.mcpServers.Revvy.url
$endpoint = $env:REVVY_MCP_URL
$headers = @{}
if ($Full) {
    if (-not $env:REVVY_FULL_ACCESS_TOKEN) {
        # Restore Reddy's normal project-scoped launch credential only for an
        # existing Full grant. Never change permissions or store the secret.
        $fullUrl = [Uri]$config.mcpServers.'Revvy-Full'.url
        if ($fullUrl.Host -ne '127.0.0.1' -or $fullUrl.AbsolutePath -notmatch '^/projects/([a-f0-9]{24})/mcp$') {
            throw 'Unexpected ProjectM Full route.'
        }
        $routeId = $Matches[1]
        $securityDir = Join-Path $env:APPDATA 'reddy-ide/revvy'
        $grants = Get-Content -LiteralPath (Join-Path $securityDir 'project-full-access.json') -Raw -Encoding UTF8 | ConvertFrom-Json
        $grant = $grants.full_projects.PSObject.Properties[$routeId]
        if (-not $grant -or [System.IO.Path]::GetFullPath($grant.Value.project_path).TrimEnd('\') -ne $projectRoot.TrimEnd('\')) {
            throw 'ProjectM does not have a matching existing Reddy Full grant.'
        }
        $rootCredential = [System.IO.File]::ReadAllText((Join-Path $securityDir 'full-access-token')).Trim()
        $hmac = New-Object System.Security.Cryptography.HMACSHA256
        try {
            $hmac.Key = [System.Text.Encoding]::UTF8.GetBytes($rootCredential)
            $payload = [System.Text.Encoding]::UTF8.GetBytes("revvy-project-full-v1`0$routeId")
            $env:REVVY_FULL_ACCESS_TOKEN = ([System.BitConverter]::ToString($hmac.ComputeHash($payload))).Replace('-', '').ToLowerInvariant()
        } finally {
            $hmac.Dispose()
            $rootCredential = $null
        }
    }
    $endpoint = $config.mcpServers.'Revvy-Full'.url
    $env:REVVY_MCP_URL = $endpoint
    $env:REVVY_MCP_FULL_URL = $endpoint
    $headers.Authorization = 'Bearer ' + $env:REVVY_FULL_ACCESS_TOKEN
}
$rpcParams = @{}
if ($Tool) {
    $Method = 'tools/call'
    $arguments = @{}
    if ($ArgumentsFile) {
        $arguments = Get-Content -LiteralPath $ArgumentsFile -Raw -Encoding UTF8 | ConvertFrom-Json
    }
    $rpcParams = @{ name = $Tool; arguments = $arguments }
}
$body = @{ jsonrpc = '2.0'; id = 1; method = $Method; params = $rpcParams } | ConvertTo-Json -Depth 30
Invoke-RestMethod -Uri $endpoint -Headers $headers -Method Post -ContentType 'application/json; charset=utf-8' -Body ([System.Text.Encoding]::UTF8.GetBytes($body)) | ConvertTo-Json -Depth 30
