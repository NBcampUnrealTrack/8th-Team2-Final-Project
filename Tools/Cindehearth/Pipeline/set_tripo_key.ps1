# Run locally. The key is never written to this repository or printed.
$tripoSecure = Read-Host 'Tripo API key (hidden input)' -AsSecureString
$tripoPointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($tripoSecure)
try {
    $tripoValue = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($tripoPointer).Trim()
    if ([string]::IsNullOrWhiteSpace($tripoValue)) { throw 'API key was empty.' }
    [Environment]::SetEnvironmentVariable('TRIPO_API_KEY', $tripoValue, 'User')
    Write-Host 'TRIPO_API_KEY saved to your local user environment. No editor restart required for this MCP adapter.'
}
finally {
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($tripoPointer)
    $tripoValue = $null
    $tripoSecure.Dispose()
}
