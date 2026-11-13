# Titan-Network-Filter PowerShell SDK

PowerShell7.4+ Windows x64. `Install-Module TitanNetworkFilter -Scope CurrentUser`.

```powershell
$client=Connect-TitanNetworkFilterSDK
try { Invoke-TitanNetworkFilterSDK -Client $client -Operation status } finally { Disconnect-TitanNetworkFilterSDK -Client $client }
```

Use -Arguments with a hashtable matching [schema](docs/API.md). Native errors retain their Windows code through the inner exception. Matching driver installed separately.
