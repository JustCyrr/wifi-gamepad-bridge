# Contributing to Wi-Fi Gamepad Bridge

Thank you for your interest in improving Wi-Fi Gamepad Bridge! We welcome bug reports, feature proposals, and code contributions from the community.

---

## Code of Conduct

Please be respectful, collaborative, and considerate in all interactions within issues, pull requests, and discussions.

---

## How to Contribute

### 1. Reporting Bugs
* Search existing issues to ensure the bug has not already been reported.
* Open an issue using the Bug Report template.
* Include detailed information:
  - Windows version and build number.
  - Android device model and OS version.
  - Controller model (e.g. Xbox Series Wireless, DualSense, 8BitDo).
  - Network type (2.4 GHz vs 5 GHz Wi-Fi, Ethernet).
  - Clear steps to reproduce the issue.

### 2. Proposing Features
* Open an issue using the Feature Request template to discuss new ideas before writing code.
* Clearly explain the motivation, use case, and proposed architecture.

### 3. Submitting Pull Requests
1. Fork the repository and create a descriptive branch:
   ```bash
   git checkout -b feature/improved-haptics
   ```
2. Follow the codebase conventions:
   - **PC Server (C++20):** Use `clang-format` or standard Microsoft/Allman styling. Keep functions clean, avoid heap allocations in the high-frequency packet processing path, and maintain non-blocking socket operations.
   - **Android App (Kotlin):** Follow idiomatic Kotlin style. Maintain coroutine safety, verify lifecycle boundaries, and keep the deterministic pacing thread jitter-free.
3. Test your changes thoroughly on real hardware before submitting:
   - Verify 0% circularity error on gamepad test utilities.
   - Verify that polling rate stays consistent at 125 Hz without packet bursts.
   - Verify force feedback / rumble responsiveness.
4. Submit the Pull Request with a clear description of the modifications made.
