# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 1.0.x   | :white_check_mark: |
| < 1.0   | :x:                |

---

## Network Architecture & Threat Model

Wi-Fi Gamepad Bridge is architected for **ultra-low-latency local LAN input streaming**:

* **Unencrypted UDP Transport:** In order to achieve consistent sub-2ms network latency and deterministic 125 Hz pacing without packet buffering or handshake stalls, input state is sent via raw binary UDP packets over port `52150`.
* **Local Subnet Operation:** This software is designed exclusively for deployment inside private, trusted home networks (WPA2/WPA3 Wi-Fi or Ethernet).
* **PIN Authentication:** A 6-digit numeric PIN is generated at server startup. The Android client must provide this token during the session handshake to prevent accidental hijacking from another device on the same subnet.
* **Public Internet Warning:** Never configure router NAT port-forwarding to expose port `52150` directly to the public internet. For remote gaming across WAN, always tunnel traffic through an authenticated VPN solution such as **Tailscale** or **WireGuard**.

---

## Reporting a Vulnerability

If you discover a security vulnerability or security bug in Wi-Fi Gamepad Bridge:

1. **Do not disclose the issue publicly** in GitHub Issues or public forums.
2. Please open a [Private Vulnerability Advisory](https://github.com/) on the GitHub repository or contact the project maintainers directly.
3. Include detailed steps to reproduce the issue along with platform information (Windows version, Android version, network topology).
4. Maintainers will investigate the report and respond with an acknowledgment and remediation plan within a reasonable timeframe.
