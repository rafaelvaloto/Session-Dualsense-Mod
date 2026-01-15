<div align="center">

# Gaming DualSense Native Mods
### Audio Haptics & Adaptive Triggers (USB/Bluetooth)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-Windows-blue)](https://www.microsoft.com/windows)
[![Powered By](https://img.shields.io/badge/Powered%20By-GamepadCore-crimson)](https://github.com/rafaelvaloto/gamepad-core)
[![Controller](https://img.shields.io/badge/Support-DualSense-white?logo=playstation)](https://www.playstation.com/)

**Bring Next-Gen Features to PC Games via Native Injection.**
<br/>
<i>No paid software required. No background apps. Just pure C++ performance.</i>

</div>

---

## 🚀 About The Project

This project brings **native DualSense Audio Haptics support over Bluetooth** to PC games using **[GamepadCore](https://github.com/rafaelvaloto/gamepad-core)**.

Unlike other solutions that require paid software (like DSX) running in the background, this logic is **embedded directly into the game process**.

### ✨ Key Features
* 🔊 **Wireless Audio Haptics:** Captures game audio loopback via WASAPI and converts it to HD vibration in real-time. Works flawlessly over Bluetooth.
* ⚡ **Zero Latency:** Direct communication with the controller's HID endpoints bypassing standard Windows driver limitations.
* 🎮 **Plug & Play:** No complex configuration needed. Drop the files, launch the game.

---

## 🛹 Session: Skate Sim Mod (Special Edition)

A tailored experience specifically designed for *Session: Skate Sim*. This is not just a generic vibe; it hooks into the game's physics engine.

### Features
* **Trigger Resistance (Truck Physics):** The Adaptive Triggers stiffen dynamically to simulate the resistance of your skate trucks. The harder you turn, the harder the trigger gets.
* **Texture Haptics:** Feel the pop of the board, the texture of the ground, and the impact of landing through high-definition audio haptics.

### 📥 Installation
> [!IMPORTANT]
> **Prerequisite:** You must download and install **[ViGEmBus](https://github.com/nefarius/ViGEmBus/releases)** first. This is required for virtual controller input emulation.

1.  Download the latest `session-dualsense-mod.zip` from **[Releases](../../releases)**.
2.  Navigate to your game folder, typically located at:
    `...\SessionGame\Binaries\Win64\`
3.  Extract the contents of the zip file directly into this folder.
4.  **Verify your folder structure:**

    ```text
    SessionGame\Binaries\Win64\
    ├── dxgi.dll                 <-- The Loader
    ├── session.exe              <-- Game Executable
    └── UnrealModPlugins\
        └── session-dualsense-mod.dll
    ```

5.  Connect your DualSense (USB or Bluetooth) and launch the game!


## 🛠️ For Developers: Build Your Own

Want to add native DualSense support to *Cyberpunk 2077*, *Elden Ring*, or your own engine? These mods are built on top of the **GamepadCore** library.

### How it works
1.  **Audio Capture:** The DLL hooks into WASAPI (Windows Audio Session API) to capture the output loopback.
2.  **Processing:** `GamepadCore` processes the audio buffer into haptic data.
3.  **Bluetooth Stream:** The processed buffer is sent directly to the DualSense HID endpoints via a custom implementation.

### Usage Example
To create a custom mod:
1.  Include `GamepadCore` in your C++ project.
2.  Initialize the DualSense instance.
3.  Use the `IGamepadAudioHaptics` interface to feed the buffer.

Check out the [GamepadCore Repository](https://github.com/rafaelvaloto/gamepad-core) for documentation and examples.

---

## ⭐ Credits & Acknowledgments

* **Core Library:** Built with **[Gamepad-Core](https://github.com/rafaelvaloto/gamepad-core)**.
* **ViGEmBus:** Special thanks to **[nefarius](https://github.com/nefarius)** for the incredible work on virtual gamepad emulation drivers.
* **Community:** Thanks to the modding community for pushing the boundaries of what's possible on PC.

## ⚖️ Legal & Trademarks

This software is an independent project and is **not** affiliated with Sony Interactive Entertainment Inc., Nacon, creā-ture Studios, or any of their subsidiaries.

* *PlayStation, DualSense, and DualShock are trademarks of Sony Interactive Entertainment Inc.*
* *Session: Skate Sim is a trademark of Nacon and creā-ture Studios.*
* *Unreal Engine is a trademark of Epic Games, Inc.*

## 📜 License

This project is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for details.
