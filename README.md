<div align="center">

# 🛹 Session: Skate Sim - Native DualSense Mod
### Audio Haptics & Adaptive Triggers (USB/Bluetooth)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-Windows-blue)](https://www.microsoft.com/windows)
[![Powered By](https://img.shields.io/badge/Powered%20By-GamepadCore-crimson)](https://github.com/rafaelvaloto/gamepad-core)
[![Controller](https://img.shields.io/badge/Support-DualSense-white?logo=playstation)](https://www.playstation.com/)

**Bring Next-Gen Features to PC Games via Native Injection.**
<br/>
<i>No paid software required. No background apps. Just pure C++ performance.</i>

</div>


This mod brings **native DualSense support** to Session: Skate Sim on PC. It communicates directly with the controller to provide next-gen features via Bluetooth or USB, without requiring DS4Windows or paid software.

## ✨ Features
* **Audio-Based Haptics:** Feel the texture of the ground, grinds, and impacts through the controller's voice coil actuators.
* **Adaptive Triggers:**
   * **Turning Resistance:** Triggers stiffen based on your board's inclination and truck tightness (turning physics).
   * **Impact Feedback:** Reacts to landing heavy drops.
* **Native Connection:** Works via standard Bluetooth or USB.

---

## ⚠️ IMPORTANT: Disable Steam Input
For this mod to work, the game must communicate directly with your DualSense. Steam Input blocks this connection.

1.  Open your Steam Library.
2.  Right-click on **Session: Skate Sim** -> **Properties**.
3.  Go to the **Controller** tab.
4.  In the dropdown menu, select **Disable Steam Input**.

*(If your controller light turns off, just press the PS button to reconnect it).*

---

## 📥 Installation

1.  Download the latest `session-dualsense-mod.zip` from **[Releases](https://github.com/rafaelvaloto/Session-Dualsense-Mod/releases/tag/v0.0.10)**.

2.  Navigate to your game folder:
   * **Steam:**
     `...\SteamLibrary\steamapps\common\Session Skate Sim\SessionGame\Binaries\Win64\`
   * **Epic Games:**
     `...\Epic Games\SessionSkateSim\SessionGame\Binaries\Win64\`

3.  **Extract the files:**
   * **Option A: I use Illusory / Mod Manager:**
     Copy **ONLY** the `UnrealModPlugins` folder into the directory above.
     ❌ **DO NOT** overwrite/copy `dxgi.dll` (you already have it).

   * **Option B: I don't use any mods:**
     Copy **BOTH** `dxgi.dll` and the `UnrealModPlugins` folder into the directory above.

4.  **Verify your folder structure:**
    ```text
    SessionGame\Binaries\Win64\
    ├── dxgi.dll                 <-- The Loader (Only needed if you DON'T use Illusory)
    ├── session.exe              <-- Game Executable
    └── UnrealModPlugins\
        └── session-dualsense-mod.dll
    ```

5.  Connect your DualSense, launch the game, and enjoy!

## 🎧 Recommended Audio Settings (Critical for Haptics)
Since this mod uses **Audio-Based Haptics**, the controller's vibration is driven directly by the game's sound frequency. To feel the texture of the ground (wood, concrete, brick) clearly, follow these steps:

1.  Go to **Options > Audio**.
2.  **Music Volume:** Turn it **DOWN** or completely **OFF** (0%).
   * *Reason:* Loud music creates constant "noise" in the haptics, making it harder to feel the subtle wheel vibrations.
3.  **SFX / Board / Environment Volume:** Keep these **HIGH** (80-100%).
   * *Reason:* This ensures the "Surface Sounds" are the main source of vibration, giving you that crisp feeling of rolling over different terrains.

## 🛠️ Compatibility
* **Conflict Warning:** Do not use DS4Windows, DSX, or Steam Input simultaneously with this mod, as they will fight for control of the device.
* **Safe to use with Illusory:** This mod is compatible with the Illusory Mod Unlocker loader.

---

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
