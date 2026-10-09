# 🎮 Wi-Fi Gamepad Bridge

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6.svg?logo=windows)](https://microsoft.com)
[![Platform: Android](https://img.shields.io/badge/Platform-Android%208.0%2B-3DDC84.svg?logo=android)](https://android.com)
[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-00599C.svg?logo=c%2B%2B)](https://isocpp.org)
[![Kotlin](https://img.shields.io/badge/Language-Kotlin-7F52FF.svg?logo=kotlin)](https://kotlinlang.org)

**Wi-Fi Gamepad Bridge** is an ultra-low-latency, zero-bloat wireless input forwarder designed to use Bluetooth gamepads connected to an Android smartphone or tablet directly on a Windows PC without requiring a Bluetooth adapter on the PC.

Inspired by the controller forwarding subsystem of Moonlight and Sunshine, but stripped of all video, audio, and compression encoding overhead to deliver pure input packets with **sub-2ms network round-trip time (RTT)** and rock-solid **125 Hz deterministic pacing**.

---

## ⚡ Key Highlights

* **Ultra-Low Latency Binary Protocol:** Custom 28-byte raw UDP packet format with sequence indexing, microsecond hardware timestamps, and zero JSON/serialization overhead.
* **Deterministic 125 Hz Clock Pacing:** Android sender runs on a dedicated high-priority audio-tier thread (`THREAD_PRIORITY_URGENT_AUDIO`) timed using `System.nanoTime()` sleep-spin loops, delivering steady 8ms intervals with 0% packet loss.
* **Native Virtual XInput Emulation:** Integrates with the [ViGEmBus](https://github.com/nefarius/ViGEmBus) kernel-mode driver to present a fully functional virtual Xbox 360 controller recognized out-of-the-box by Steam, Xbox Game Pass, Epic Games, and all Windows titles.
* **Dual-Motor Force Feedback (Rumble):** Real-time reverse UDP rumble streaming from games back to the Android controller motors using modern `VibratorManager` (`CombinedVibration`) and device haptics.
* **0% Analog Stick Circularity Error:** Radial deadzone normalization and precision axis remapping eliminate circularity clipping, dead-angles, and sticky axes.
* **Frutiger Aero Crystal Glass HUD:** Lightweight Windows GUI featuring native DWM Acrylic blur-behind, real-time stick telemetry, and automatic minimize-to-system-tray (no taskbar clutter).
* **Automatic LAN Discovery & PIN Pairing:** UDP broadcast discovery pairs Android and PC seamlessly on your local Wi-Fi with a 6-digit session PIN.

---

## 🏛️ System Architecture

```text
┌────────────────────────┐
│ Physical Gamepad       │ (Xbox, DualSense, 8BitDo, etc.)
└───────────┬────────────┘
            │ Bluetooth HID Input
            ▼
┌────────────────────────┐
│ Android Device (Client)│
│  - 125 Hz Paced Thread │
│  - Radial Normalization│
│  - Dual-Motor Haptics  │
└───────────┬────────────┘
            │
            │  28-Byte Binary UDP / Wi-Fi LAN
            │  (Sub-2ms Network Round-Trip Time)
            ▼
┌────────────────────────┐
│ Windows Server (PC)    │
│  - High-Priority WinSock
│  - Acrylic Glass HUD   │
│  - System Tray Daemon  │
└───────────┬────────────┘
            │ ViGEmBus Kernel API
            ▼
┌────────────────────────┐
│ Virtual Xbox 360 Dev   │
└───────────┬────────────┘
            │ Native XInput
            ▼
┌────────────────────────┐
│ Games / Steam / Apps   │
└────────────────────────┘
```

---

## 📦 Packet Protocol Specification (28 Bytes)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Magic ('W')  |  Type (0x01)  |         Session Token         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Sequence Counter                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Hardware Timestamp (us)                    |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Button Bitmask        |  Trigger Left | Trigger Right |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Thumb LX             |          Thumb LY             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Thumb RX             |          Thumb RY             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

---

## 🚀 Quick Start Guide

### 1. Prerequisites (Windows PC)
1. **Windows 10 / 11 (64-bit)**.
2. **ViGEmBus Driver:** Download and install the latest official driver release from [Nefarius ViGEmBus Releases](https://github.com/nefarius/ViGEmBus/releases).

### 2. PC Server Setup
1. Launch `WiFiGamepadServer.exe`.
2. The server will display the local IP, port `52150`, and a 6-digit **Pairing PIN**.
3. *Tip:* Click the minimize button (`_`) at any time to hide the server into the **Windows System Tray** (notification area). Double-click the tray icon to restore it.

### 3. Android Client Setup
1. Install and launch `WiFiGamepad.apk` on your Android device (Android 8.0 or later).
2. Pair your Bluetooth gamepad with your Android device.
3. Tap **Scan for PCs** to detect your computer automatically on the local Wi-Fi, or enter the PC's local IP address manually.
4. Tap **Connect**. Inputs will immediately start streaming with low-latency haptic force feedback enabled.

---

## 🛠️ Building From Source

### Building the Windows Server
* **Requirements:** Visual Studio 2022 / Build Tools (C++20, MSVC toolset v143+), CMake 3.20+.

```powershell
# Navigate to the server folder
cd pc-server

# Generate CMake configuration (Release mode)
cmake -B build -G "Visual Studio 17 2022" -A x64

# Compile the release binary
cmake --build build --config Release
```
The output binary will be generated at `pc-server/build/Release/WiFiGamepadServer.exe`.

### Building the Android App
* **Requirements:** JDK 17+, Android SDK (API 34 / Build-Tools 34.0.0).

```bash
# Navigate to the android folder
cd android-app

# Build debug APK
./gradlew assembleDebug
```
The generated APK will be located at `android-app/app/build/outputs/apk/debug/app-debug.apk`.

---

## 🔒 Security & Safe Usage Notice

* **Local Network (LAN) Scope:** Designed exclusively for secure private home networks (WPA2/WPA3 Wi-Fi). Traffic over UDP port `52150` is raw binary for maximum throughput.
* **Public Internet Warning:** Never port-forward UDP port `52150` across your public router (WAN) without an encrypted tunnel (WireGuard, Tailscale).
* For detailed threat model analysis and vulnerability reporting guidelines, consult [SECURITY.md](SECURITY.md).

---

## ⚠️ Anti-Cheat & Legal Disclaimer

* **Competitive Games Notice:** Emulated virtual controllers via ViGEmBus are completely standard for offline titles, Steam Input, and single-player games. However, competitive multiplayer titles with strict kernel-level anti-cheat (e.g. *Riot Vanguard*, *BattlEye*, *Easy Anti-Cheat*, *Ricochet*) have strict heuristics regarding virtual input devices. **Use at your own discretion in online competitive games; authors assume no liability for account suspensions or game bans.**
* For complete terms, limitations of liability, and health & safety disclosures, read [DISCLAIMER.md](DISCLAIMER.md).

---

## 📄 License & Third-Party Credits

* This project is licensed under the [MIT License](LICENSE).
* ViGEmClient is open-source software by Benjamin Höglinger-Stelzer (Nefarius) under the MIT License.
* See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for complete attributions and license texts.

---

## 🏷️ Trademarks

*Xbox, Windows, and XInput are trademarks of Microsoft Corporation. PlayStation, DualShock, and DualSense are trademarks of Sony Interactive Entertainment Inc. Nintendo Switch is a trademark of Nintendo Co., Ltd. Android is a trademark of Google LLC. Steam is a trademark of Valve Corporation. This project is an independent community project not affiliated with, sponsored by, or endorsed by any of these entities.*
