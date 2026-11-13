# Titan-Network-Filter Python SDK

`pip install titan-network-filter`

Windows x64 Python3.10+. No driver is installed by this package.

```python
from titan_network_filter import Client
with Client() as client:
    info = client.call("status")
```

[Operation schema and API](docs/API.md). Check errors and the product licensing state before device creation.
