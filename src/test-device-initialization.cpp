#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>

#include "GCore/Interfaces/IPlatformHardwareInfo.h"
#include "GCore/Templates/TGenericHardwareInfo.h"
#include "GCore/Templates/TBasicDeviceRegistry.h"
#include "GCore/Interfaces/ISonyGamepad.h"
#include "../lib/Gamepad-Core/Examples/Adapters/Tests/test_device_registry_policy.h"
#include "../lib/Gamepad-Core/Examples/Platform_Windows/test_windows_hardware_policy.h"

using namespace GamepadCore;
using TestHardwareInfo = Ftest_windows_platform::Ftest_windows_hardware;
using TestDeviceRegistry = GamepadCore::TBasicDeviceRegistry<Ftest_device_registry_policy>;

int main() {
    std::cout << "--- DualSense Initialization Test ---" << std::endl;

    // 1. Initialization Hardware Info
    auto HardwareImpl = std::make_unique<TestHardwareInfo>();
    IPlatformHardwareInfo::SetInstance(std::move(HardwareImpl));

    // 2. Initialization Registry Devices
    auto Registry = std::make_unique<TestDeviceRegistry>();
    Registry->Policy.deviceId = 0;

    std::cout << "[System] Search devices DualSense..." << std::endl;
    Registry->RequestImmediateDetection();
    Registry->PlugAndPlay(2.0f);

    ISonyGamepad* Gamepad = nullptr;
    for (int i = 0; i < 50; ++i) { // load connection
        Gamepad = Registry->GetLibrary(0);
        if (Gamepad && Gamepad->IsConnected()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!Gamepad || !Gamepad->IsConnected()) {
        std::cerr << "[Error] No DualSense controller detected. Check your USB or Bluetooth connection." << std::endl;
        return 1;
    }

    std::cout << "[Success] Connected controller via"
              << (Gamepad->GetConnectionType() == EDSDeviceConnection::Bluetooth ? "Bluetooth" : "USB") 
              << std::endl;

    // 4. Teste de Lightbar (Cores básicas)
    std::cout << "[Tests] Changing Lightbar Colors..." << std::endl;
    
    DSCoreTypes::FDSColor colors[] = {
        {255, 0, 0},   // Red
        {0, 255, 0},   // Green
        {0, 0, 255},   // Blue
        {200, 160, 80} // Config (Session: Skater)
    };

    for (const auto& color : colors) {
        std::cout << "  Define color: R=" << (int)color.R << " G=" << (int)color.G << " B=" << (int)color.B << std::endl;
        Gamepad->SetLightbar(color);
        Gamepad->UpdateOutput();
    	std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    // 5. Teste de Player LED
    std::cout << "[Test] Testing Player LED..." << std::endl;
    Gamepad->SetPlayerLed(EDSPlayer::One, 255);
    Gamepad->UpdateOutput();

    // 6. Teste de Gatilhos Adaptáveis (Resistência)
    std::cout << "[Test] Applying resistance to the triggers (press L2/R2 to test)..." << std::endl;
    auto Trigger = Gamepad->GetIGamepadTrigger();
    if (Trigger) {
        Trigger->SetResistance(0, 255, EDSGamepadHand::Left);
        Trigger->SetResistance(0, 255, EDSGamepadHand::Right);
        Gamepad->UpdateOutput();
        
        std::cout << "  Applied resistance. Waiting 5 seconds..." << std::endl;
    	std::this_thread::sleep_for(std::chrono::seconds(5));
        
        std::cout << "  Clearing the effects of triggers..." << std::endl;
        Trigger->StopTrigger(EDSGamepadHand::Left);
        Trigger->StopTrigger(EDSGamepadHand::Right);
        Gamepad->UpdateOutput();
    } else {
        std::cerr << "  [Notice] Trigger interface not available.." << std::endl;
    }

    std::cout << "--- Test Completed ---" << std::endl;
    Registry.reset();

    return 0;
}
