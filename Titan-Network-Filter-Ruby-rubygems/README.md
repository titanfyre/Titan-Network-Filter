# Titan-Network-Filter Ruby SDK

`gem install titan-network-filter`

Windows x64 Ruby3.1+.

```ruby
require "titan_network_filter"
TitanNetworkFilterSDK::Client.open { |client| p client.call("status") }
```

[API and operation schema](docs/API.md). Matching driver installed separately.
