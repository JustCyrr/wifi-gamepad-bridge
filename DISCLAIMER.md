# Legal Disclaimer & Terms of Use

**Please read this document carefully before using or contributing to the Wi-Fi Gamepad Bridge project.**

---

## 1. "AS IS" Software & Limitation of Liability

Wi-Fi Gamepad Bridge is provided as free, open-source software under the **MIT License** on an **"AS IS" and "AS AVAILABLE" basis**, without warranty of any kind, whether express or implied, including but not limited to the warranties of merchantability, fitness for a particular purpose, non-infringement, or course of performance.

**Under no circumstances shall the authors, maintainers, contributors, or copyright holders be held liable for:**
* Any direct, indirect, incidental, special, exemplary, or consequential damages (including, but not limited to, procurement of substitute goods or services; loss of use, data, or profits; business interruption).
* Any damage to computer hardware, mobile devices, battery health, vibration motors, Wi-Fi routers, or peripherals resulting from using this software.
* Any security breaches, data interception, or unauthorized access occurring over unencrypted local wireless networks.
* Any software conflicts, system crashes, blue screens of death (BSOD), or driver incompatibilities.

By downloading, compiling, installing, or executing any part of this software, you assume all risks associated with its operation.

---

## 2. Competitive Gaming & Anti-Cheat Advisory

Wi-Fi Gamepad Bridge interacts with the Windows operating system via the **ViGEmBus** virtual device driver to expose an emulated virtual Xbox 360 controller (`XInput` device).

* **Single-Player & Offline Games:** Completely safe and standard. Virtual controller emulation is standard practice for Steam Input, controller remappers, and retro emulators.
* **Online Multiplayer & Competitive Games:** Certain games protected by strict kernel-mode or proprietary anti-cheat systems (e.g., *Riot Vanguard*, *Easy Anti-Cheat*, *BattlEye*, *Ricochet*, *Valve Anti-Cheat*, *Tencent ACE*) enforce proprietary heuristics regarding virtual input devices, virtual bus drivers, or simulated peripherals.

> ⚠️ **IMPORTANT NOTICE:**  
> The authors and maintainers do **NOT** condone, promote, or support cheating, unfair advantage, or bypassing game policies. Using any virtual controller software in competitive environments is strictly at your own risk. The authors **assume no liability whatsoever** for in-game bans, account restrictions, suspensions, or rank resets imposed by game publishers or anti-cheat providers.

---

## 3. Network & Security Notice

Wi-Fi Gamepad Bridge communicates via high-speed, binary **UDP packets on port 52150** without packet-level TLS/SSL encryption in order to achieve sub-millisecond latency (sub-2ms round-trip time) and 125 Hz deterministic pacing.

* **Trusted Private Networks Only:** This software is intended solely for private, secure home Wi-Fi or local area networks (LAN).
* **Do NOT Port-Forward to the Public Internet:** Never forward UDP port `52150` on your router to the public internet (WAN). If remote connection outside your home network is required, you must use a secure encrypted virtual private network (e.g., **WireGuard**, **Tailscale**, or **ZeroTier**).
* **PIN Authentication:** The 6-digit pairing PIN is designed to prevent accidental cross-connections between devices on the same local subnet; it is not a cryptographic signature.

---

## 4. Hardware Safety & Haptic Feedback Notice

* **Continuous Motor Operation:** Extreme or continuous force feedback (vibration) over extended gaming sessions may cause mobile device vibration actuators or gamepad motors to warm up or consume elevated battery levels.
* Built-in safeguards pace and throttle rumble durations, but users should exercise normal care and allow devices to cool down if extended gaming causes noticeable warming.

---

## 5. Trademark & Copyright Disclaimers

All product names, logos, brands, trademarks, and registered trademarks mentioned in this repository are the property of their respective owners.

* **Microsoft®, Windows®, Xbox®, Xbox 360®, Xbox One®, Xbox Series X/S®, and XInput®** are registered trademarks of **Microsoft Corporation** in the United States and other countries.
* **Sony®, PlayStation®, DualShock®, and DualSense®** are registered trademarks of **Sony Interactive Entertainment Inc.**
* **Nintendo® and Nintendo Switch®** are registered trademarks of **Nintendo Co., Ltd.**
* **Android™, Google Play™, and the Android robot** are trademarks of **Google LLC**.
* **Steam® and the Steam logo** are trademarks and/or registered trademarks of **Valve Corporation**.
* **ViGEm® and ViGEmBus** are open-source technologies created by **Benjamin Höglinger-Stelzer (Nefarius)**.

The use of these trademarks does not imply any affiliation, sponsorship, endorsement, or licensing by their respective owners. Wi-Fi Gamepad Bridge is an independent, non-commercial, open-source project created strictly for interoperability purposes.
