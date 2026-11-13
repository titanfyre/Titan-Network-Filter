# Titan-Network-Filter operation schema

The unchanged product C++ SDK is called directly. Operations: `check`, `make_rule`, `start`, `set_rules`, `stop`, `status`, `read`, `event_address`. The dispatch source in `Bridge.cpp` lists exact argument keys and maps every operation to the original public SDK. Protocol structs retain their public-header field names, including case. UInt64 and Int64 fields are decimal strings; fixed arrays accept at most their capacity and omitted fields initialize to zero. Unknown fields and out-of-range values are rejected. SDK validators apply original protocol constraints.

Wire types and fields:

- `NF_REQUEST`: Magic, Version, Size, Options
- `NF_RULE`: ProcessId, Id, LocalFirst, LocalLast, RemoteFirst, RemoteLast, Family, Direction, Protocol, Action, RemotePrefix, Reserved, RemoteAddress
- `NF_POLICY`: Magic, Version, Size, Count, Rules
- `NF_EVENT`: Sequence, TimestampQpc, ProcessId, Generation, Flags, RuleId, LocalPort, RemotePort, Family, Stage, Protocol, Direction, LocalAddress, RemoteAddress, PathLength, Decision, Application
- `NF_STATUS`: Magic, Version, Size, Active, QpcFrequency, Produced, Dropped, Available, Capacity, Options, RuleCount, BytesIn, BytesOut, PacketsIn, PacketsOut, Blocked, Continued, Generation
- `NF_BATCH`: Magic, Version, Size, Count, Dropped, Events

Only explicit start/connect/policy calls start the corresponding operation. The C ABI session constructor is inert; one active session per product DLL. Always close. Capture receives are bounded when the native API accepts a timeout. SDK original licensing/trial checks remain unchanged. No driver is installed by packages.

Tests exercise synthetic wire inputs using the real production SDK validation/formatting helpers, rejected arguments, exact unsigned values and inert session lifecycle. No live capture, policy application, storage fault, traffic modification or driver activation is performed by these tests. Operational driver paths require a separate integration environment.

## Language client

The client calls the native SDK directly. Use the call method with an operation name and argument object. Calls are synchronous; only operations exposing a receive timeout are capped at100ms; use an application worker for responsive UI/event loops. Explicitly close the client after use. 64-bit identifiers and counters are decimal strings. Native errors expose their Windows code. Matching driver and licensing are separate requirements.
