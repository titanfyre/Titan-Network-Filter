# Validation

This is a signed evaluation build, not production certification or a guarantee of
compatibility with all Windows versions and network/security products.

The user-mode Release build uses MSVC W4/WX, CFG, ASLR and NX. AddressSanitizer
also exercises malformed wire records, rule validation, CIDR normalization and
invalid policy cases without creating filters. The private kernel is built with
WDK 26100, PREfast (zero reported defects), KMDF 1.31 and CFG. InfVerif `/w` and
`/u`, Inf2Cat and CAB extraction/hash checks pass. A separate private executable
exercises the exact kernel matcher and ring for two million iterations.

Windows 10 build 19045 x64 live VM campaigns passed normally and under targeted
standard Driver Verifier. The final Verifier run passed 24 cases: TCP/UDP on
IPv4/IPv6, outbound/inbound rule enforcement, kernel-attributed block evidence,
ordered Continue exceptions, clearing policies and Stop/Start. Another 200 TCP
loopback setup/transfer operations passed. Abrupt termination of an owned policy
client restored local TCP connectivity. No log drops occurred in that short run.
The original CAB image was compared to the guest-signed installed image, excluding
only signature/checksum fields. Unload and owned-node removal passed. The VM was
shut down normally and its baseline restored; all nine historical snapshots were
preserved. No driver was installed on the host. This is bounded guest-only test
signing, not a returned Microsoft package, extended stress or universal support.

Remaining qualification includes Windows
11, HVCI / Secure Boot, VPN/IPsec/firewall coexistence, BFE restarts, physical
Ethernet/Wi-Fi traffic and long sustained high-rate workloads. Offline matcher
timings and local socket round trips must not be presented as driver overhead or
end-to-end network latency.

## Returned Microsoft-signed package validation

The returned 0.1.0.0 package was verified against the submitted image: unchanged INF and Authenticode-protected SYS content, trusted Microsoft catalog, and INF/SYS catalog membership. Both self-contained installers embed the exact verified files.

The actual console installer, live functionality and uninstall/unload passed on Windows 10 build 19045 x64 twice: normally and under standard Driver Verifier targeted at this product and the other two evaluation drivers. Guest test-signing was disabled; no lab certificate was used. 24 IPv4/IPv6 TCP/UDP policy cases, 200 TCP connections, exception/clear/stop-start and abrupt-client-exit recovery per campaign. No new minidump was observed. Both campaigns restored the original VM and preserved its historical checkpoints. No driver was installed on the development host.

These are bounded VM tests, not a Windows 11, Secure Boot/HVCI, physical hardware, long-duration or customer-deployment qualification. Installer UI resources are byte-verified; the automated install/uninstall runs used the console setup.
