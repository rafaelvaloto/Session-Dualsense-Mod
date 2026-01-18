#include <windows.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <queue>

#include "GImplementations/Utils/GamepadAudio.h"
using namespace FGamepadAudio;

#include "GCore/Interfaces/IPlatformHardwareInfo.h"
#include "GCore/Templates/TGenericHardwareInfo.h"
#include "GCore/Templates/TBasicDeviceRegistry.h"
#include "GCore/Types/Structs/Context/DeviceContext.h"
#include "../Examples/Adapters/Tests/test_device_registry_policy.h"

#ifdef USE_VIGEM
#include "../Examples/Platform_Windows/ViGEmAdapter/ViGEmAdapter.h"
#endif

#if _WIN32
#include "../Examples/Platform_Windows/test_windows_hardware_policy.h"
using TestHardwarePolicy = Ftest_windows_platform::Ftest_windows_hardware_policy;
using TestHardwareInfo = Ftest_windows_platform::Ftest_windows_hardware;
#endif

using namespace GamepadCore;
using TestDeviceRegistry = GamepadCore::TBasicDeviceRegistry<Ftest_device_registry_policy>;
// Definição do ponteiro da função original
typedef UINT (WINAPI* PGETRAWINPUTDEVICELIST)(PRAWINPUTDEVICELIST, PUINT, UINT);
PGETRAWINPUTDEVICELIST Original_GetRawInputDeviceList = NULL;

// O VID que queremos esconder (Sony DualSense)
const int HID_VID_SONY = 0x054C;

// -----------------------------------------------------------------------------
// SUA FUNÇÃO MODIFICADA (O DETOUR)
// -----------------------------------------------------------------------------
UINT WINAPI My_GetRawInputDeviceList(PRAWINPUTDEVICELIST pRawInputDeviceList, PUINT puiNumDevices, UINT cbSize) {

    // 1. Chama a função original do Windows para pegar a lista real
    // Se Original_GetRawInputDeviceList for NULL, algo deu errado no Hook, tenta chamar direto da API
    auto pFunc = Original_GetRawInputDeviceList ? Original_GetRawInputDeviceList : GetRawInputDeviceList;

    UINT result = pFunc(pRawInputDeviceList, puiNumDevices, cbSize);

    // Se for erro ou apenas consulta de tamanho (NULL), retorna o original
    if (result == (UINT)-1 || pRawInputDeviceList == NULL) {
        return result;
    }

    // 2. Filtragem: Vamos reconstruir a lista sem o DualSense
    UINT realCount = *puiNumDevices;
    std::vector<RAWINPUTDEVICELIST> cleanList;

    for (UINT i = 0; i < realCount; i++) {
        RAWINPUTDEVICELIST& device = pRawInputDeviceList[i];
        bool bIsBlocked = false;

        if (device.dwType == RIM_TYPEHID) {
            RID_DEVICE_INFO info;
            info.cbSize = sizeof(RID_DEVICE_INFO);
            UINT size = sizeof(RID_DEVICE_INFO);

            // Verifica o VID
            if (GetRawInputDeviceInfoA(device.hDevice, RIDI_DEVICEINFO, &info, &size) > 0) {
                if (info.hid.dwVendorId == HID_VID_SONY) {
                    bIsBlocked = true; // É o DualSense, marca para não incluir
                }
            }
        }

        if (!bIsBlocked) {
            cleanList.push_back(device);
        }
    }

    // 3. Sobrescreve o buffer original com a lista limpa
    UINT finalCount = (UINT)cleanList.size();
    for (UINT i = 0; i < finalCount; i++) {
        pRawInputDeviceList[i] = cleanList[i];
    }

    // Atualiza o contador para a Unreal não ler lixo de memória
    *puiNumDevices = finalCount;

    return finalCount;
}

// -----------------------------------------------------------------------------
// LÓGICA DE IAT HOOKING (MANUAL)
// -----------------------------------------------------------------------------
bool InstallIATHook(const char* dllName, const char* funcName, void* newFunc, void** originalFunc) {
    // Pega o módulo principal do processo (o .exe do jogo)
    HMODULE hModule = GetModuleHandle(NULL);
    if (!hModule) return false;

    // Pega os cabeçalhos do PE (Portable Executable)
    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;
    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + pDosHeader->e_lfanew);

    // Localiza a Tabela de Importação (Import Directory)
    PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hModule +
        pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

    if (pImportDesc == (PIMAGE_IMPORT_DESCRIPTOR)pNtHeaders) return false;

    // Percorre todas as DLLs importadas pelo jogo
    while (pImportDesc->Name) {
        const char* currentDllName = (const char*)((BYTE*)hModule + pImportDesc->Name);

        // Verifica se é a DLL que procuramos (ex: "USER32.dll")
        if (_stricmp(currentDllName, dllName) == 0) {

            // Pega a Thunk Table (Lista de funções importadas)
            PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)((BYTE*)hModule + pImportDesc->FirstThunk);
            PIMAGE_THUNK_DATA pOrgThunk = (PIMAGE_THUNK_DATA)((BYTE*)hModule + pImportDesc->OriginalFirstThunk);

            while (pThunk->u1.Function && pOrgThunk->u1.AddressOfData) {
                // Pega o nome da função
                PIMAGE_IMPORT_BY_NAME pImport = (PIMAGE_IMPORT_BY_NAME)((BYTE*)hModule + pOrgThunk->u1.AddressOfData);

                if (strcmp(pImport->Name, funcName) == 0) {
                    // ACHAMOS!

                    // Salva o endereço original para podermos chamar depois
                    if (originalFunc) *originalFunc = (void*)pThunk->u1.Function;

                    // Precisamos mudar a permissão da memória para escrever nela (geralmente é Read-Only)
                    DWORD oldProtect;
                    VirtualProtect(&pThunk->u1.Function, sizeof(uintptr_t), PAGE_READWRITE, &oldProtect);

                    // A TROCA: Substitui o ponteiro original pelo nosso
                    pThunk->u1.Function = (uintptr_t)newFunc;

                    // Restaura a permissão original
                    VirtualProtect(&pThunk->u1.Function, sizeof(uintptr_t), oldProtect, &oldProtect);

                    return true;
                }
                pThunk++;
                pOrgThunk++;
            }
        }
        pImportDesc++;
    }
    return false;
}

// -----------------------------------------------------------------------------
// FUNÇÃO DE INICIALIZAÇÃO
// -----------------------------------------------------------------------------
void InitMod() {
    // Instala o hook no USER32.dll procurando por GetRawInputDeviceList
    bool success = InstallIATHook(
        "USER32.dll",
        "GetRawInputDeviceList",
        (void*)My_GetRawInputDeviceList,
        (void**)&Original_GetRawInputDeviceList
    );

    if (success) {
        // MessageBoxA(NULL, "Hook IAT Instalado! DualSense escondido.", "Mod Loaded", MB_OK);
    	std::cout << "Hook IAT Instaled! DualSense hide." << std::endl;
    } else {
    	std::cout << "Failed to install IAT Hook." << std::endl;
    }
}

std::atomic<bool> g_Running(false);
std::atomic<bool> g_ServiceInitialized(false);
std::thread g_ServiceThread;
std::thread g_AudioThread;
std::unique_ptr<TestDeviceRegistry> g_Registry;
#ifdef USE_VIGEM
std::unique_ptr<ViGEmAdapter> g_ViGEmAdapter;
#endif
FInputContext g_LastInputState;

constexpr float kLowPassAlpha = 1.0f;
constexpr float kOneMinusAlpha = 1.0f - kLowPassAlpha;

constexpr float kLowPassAlphaBt = 1.0f;
constexpr float kOneMinusAlphaBt = 1.0f - kLowPassAlphaBt;

// Estrutura para filtro Bi-quad (EQ)
struct BiquadFilter {
	float b0, b1, b2, a1, a2;
	float x1, x2, y1, y2;

	BiquadFilter() : b0(1), b1(0), b2(0), a1(0), a2(0), x1(0), x2(0), y1(0), y2(0) {}

	void ConfigurePeaking(float sampleRate, float frequency, float q, float gainDb) {
		float a = powf(10.0f, gainDb / 40.0f);
		float omega = 2.0f * 3.14159265f * frequency / sampleRate;
		float sn = sinf(omega);
		float cs = cosf(omega);
		float alpha = sn / (2.0f * q);

		b0 = 1.0f + alpha * a;
		b1 = -2.0f * cs;
		b2 = 1.0f - alpha * a;
		float a0 = 1.0f + alpha / a;
		a1 = -2.0f * cs;
		a2 = 1.0f - alpha / a;

		b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
	}

	float Process(float in) {
		float out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
		x2 = x1; x1 = in;
		y2 = y1; y1 = out;
		return out;
	}
};


template<typename T>
class ThreadSafeQueue
{
public:
	void Push(const T& item)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mQueue.push(item);
	}

	void Push(T&& item)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mQueue.push(std::move(item));
	}

	bool Pop(T& item)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		if (mQueue.empty())
		{
			return false;
		}
		item = mQueue.front();
		mQueue.pop();
		return true;
	}

	bool Empty()
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return mQueue.empty();
	}

private:
	std::queue<T> mQueue;
	std::mutex mMutex;
};

struct AudioCallbackData
{
	ma_decoder* pDecoder = nullptr;
	bool bIsSystemAudio = false;
	float LowPassStateLeft = 0.0f;
	float LowPassStateRight = 0.0f;
	std::atomic<bool> bFinished{false};
	std::atomic<uint64_t> framesPlayed{0};
	bool bIsWireless = false;

	// Filtros para atenuação de frequências específicas (Skate sliding, etc)
	BiquadFilter FilterRailLeft, FilterRailRight;
	BiquadFilter FilterConcreteLeft, FilterConcreteRight;
	bool bFiltersConfigured = false;

	ThreadSafeQueue<std::vector<uint8_t>> btPacketQueue;
	ThreadSafeQueue<std::vector<int16_t>> usbSampleQueue;
	std::mutex usbMutex;

	std::vector<float> btAccumulator;
	std::mutex btAccumulatorMutex;
};

void AudioDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	auto* pData = static_cast<AudioCallbackData*>(pDevice->pUserData);
	if (!pData)
	{
		return;
	}

	std::vector<float> tempBuffer(frameCount * 2);
	ma_uint64 framesRead = 0;

	if (pData->bIsSystemAudio)
	{
		if (pInput == nullptr)
		{
			return;
		}

		if (!pData->bFiltersConfigured)
		{
			float sr = static_cast<float>(pDevice->sampleRate);

			pData->FilterRailLeft.ConfigurePeaking(sr, 4500.0f, 1.0f, 5.0f);
			pData->FilterRailRight.ConfigurePeaking(sr, 4500.0f, 1.0f, 5.0f);
			pData->FilterConcreteLeft.ConfigurePeaking(sr, 1200.0f, 0.7f, 6.0f);
			pData->FilterConcreteRight.ConfigurePeaking(sr, 1200.0f, 0.7f, 6.0f);
			pData->FilterConcreteLeft.ConfigurePeaking(sr, 200.0f, 0.7f, 5.0f);
			pData->FilterConcreteRight.ConfigurePeaking(sr, 200.0f, 0.7f, 5.0f);

			pData->bFiltersConfigured = true;
		}

		auto pInputFloat = static_cast<const float*>(pInput);
		for (ma_uint32 i = 0; i < frameCount * 2; i += 2)
		{
			float l = pInputFloat[i];
			float r = pInputFloat[i + 1];

			// Aplica os filtros
			l = pData->FilterRailLeft.Process(l);
			l = pData->FilterConcreteLeft.Process(l);
			r = pData->FilterRailRight.Process(r);
			r = pData->FilterConcreteRight.Process(r);

			tempBuffer[i] = l;
			tempBuffer[i + 1] = r;
		}
		framesRead = frameCount;

		if (pOutput)
		{
			std::memcpy(pOutput, tempBuffer.data(), frameCount * 2 * sizeof(float));
		}
	}
	else
	{
		if (!pData->pDecoder)
		{
			if (pOutput)
			{
				std::memset(pOutput, 0, frameCount * pDevice->playback.channels * ma_get_bytes_per_sample(pDevice->playback.format));
			}
			return;
		}

		ma_result result = ma_decoder_read_pcm_frames(pData->pDecoder, tempBuffer.data(), frameCount, &framesRead);

		if (result != MA_SUCCESS || framesRead == 0)
		{
			pData->bFinished = true;
			if (pOutput)
			{
				std::memset(pOutput, 0, frameCount * pDevice->playback.channels * ma_get_bytes_per_sample(pDevice->playback.format));
			}
			return;
		}

		if (!pData->bFiltersConfigured)
		{
			float sr = static_cast<float>(pDevice->sampleRate);
			pData->FilterRailLeft.ConfigurePeaking(sr, 4500.0f, 1.0f, 5.0f);
			pData->FilterRailRight.ConfigurePeaking(sr, 4500.0f, 1.0f, 5.0f);
			pData->FilterConcreteLeft.ConfigurePeaking(sr, 1200.0f, 0.7f, 6.0f);
			pData->FilterConcreteRight.ConfigurePeaking(sr, 1200.0f, 0.7f, 6.0f);
			pData->FilterConcreteLeft.ConfigurePeaking(sr, 200.0f, 0.7f, 5.0f);
			pData->FilterConcreteRight.ConfigurePeaking(sr, 200.0f, 0.7f, 5.0f);
			pData->bFiltersConfigured = true;
		}

		for (ma_uint64 i = 0; i < framesRead; i++)
		{
			float l = tempBuffer[i * 2];
			float r = tempBuffer[i * 2 + 1];

			l = pData->FilterRailLeft.Process(l);
			l = pData->FilterConcreteLeft.Process(l);
			r = pData->FilterRailRight.Process(r);
			r = pData->FilterConcreteRight.Process(r);

			tempBuffer[i * 2] = l;
			tempBuffer[i * 2 + 1] = r;
		}

		if (pOutput)
		{
			auto* pOutputFloat = static_cast<float*>(pOutput);
			std::memcpy(pOutputFloat, tempBuffer.data(), framesRead * 2 * sizeof(float));

			if (framesRead < frameCount)
			{
				std::memset(&pOutputFloat[framesRead * 2], 0, (frameCount - framesRead) * 2 * sizeof(float));
			}
		}
	}

	if (!pData->bIsWireless)
	{
		// std::cout << "[AppDLL] Audio Loop USB." << std::endl;
		for (ma_uint64 i = 0; i < framesRead; ++i)
		{
			float inLeft = tempBuffer[i * 2];
			float inRight = tempBuffer[i * 2 + 1];

			pData->LowPassStateLeft = kOneMinusAlpha * inLeft + kLowPassAlpha * pData->LowPassStateLeft;
			pData->LowPassStateRight = kOneMinusAlpha * inRight + kLowPassAlpha * pData->LowPassStateRight;

			float outLeft = std::clamp(inLeft - pData->LowPassStateLeft, -1.0f, 1.0f);
			float outRight = std::clamp(inRight - pData->LowPassStateRight, -1.0f, 1.0f);

			std::vector<int16_t> stereoSample = {
				static_cast<int16_t>(outLeft * 32767.0f),
				static_cast<int16_t>(outRight * 32767.0f)};
			pData->usbSampleQueue.Push(stereoSample);
		}
	}
	else
	{
		// std::cout << "[AppDLL] Audio Loop BT." << std::endl;
		{
			std::lock_guard<std::mutex> lock(pData->btAccumulatorMutex);

			const size_t maxAccumulatorFrames = 24000 * 2;
			if (pData->btAccumulator.size() > maxAccumulatorFrames)
			{
				pData->btAccumulator.clear();
			}

			for (ma_uint64 i = 0; i < framesRead; ++i)
			{
				pData->btAccumulator.push_back(tempBuffer[i * 2]);
				pData->btAccumulator.push_back(tempBuffer[i * 2 + 1]);
			}
		}

		const size_t requiredSamples = 1024 * 2;

		while (true)
		{
			std::vector<float> framesToProcess;

			{
				std::lock_guard<std::mutex> lock(pData->btAccumulatorMutex);
				if (pData->btAccumulator.size() < requiredSamples)
				{
					break;
				}

				framesToProcess.assign(pData->btAccumulator.begin(), pData->btAccumulator.begin() + requiredSamples);
				pData->btAccumulator.erase(pData->btAccumulator.begin(), pData->btAccumulator.begin() + requiredSamples);
			}

			const float ratio = 3000.0f / 48000.0f;
			const std::int32_t numInputFrames = 1024;

			std::vector<float> resampledData(128, 0.0f);

			for (std::int32_t outFrame = 0; outFrame < 64; ++outFrame)
			{
				float srcPos = static_cast<float>(outFrame) / ratio;
				std::int32_t srcIndex = static_cast<std::int32_t>(srcPos);
				float frac = srcPos - static_cast<float>(srcIndex);

				if (srcIndex >= numInputFrames - 1)
				{
					srcIndex = numInputFrames - 2;
					frac = 1.0f;
				}
				if (srcIndex < 0)
				{
					srcIndex = 0;
				}

				float left0 = framesToProcess[srcIndex * 2];
				float left1 = framesToProcess[(srcIndex + 1) * 2];
				float right0 = framesToProcess[srcIndex * 2 + 1];
				float right1 = framesToProcess[(srcIndex + 1) * 2 + 1];

				resampledData[outFrame * 2] = left0 + frac * (left1 - left0);
				resampledData[outFrame * 2 + 1] = right0 + frac * (right1 - right0);
			}

			for (std::int32_t i = 0; i < 64; ++i)
			{
				const std::int32_t dataIndex = i * 2;

				float inLeft = resampledData[dataIndex];
				float inRight = resampledData[dataIndex + 1];

				pData->LowPassStateLeft = kOneMinusAlphaBt * inLeft + kLowPassAlphaBt * pData->LowPassStateLeft;
				pData->LowPassStateRight = kOneMinusAlphaBt * inRight + kLowPassAlphaBt * pData->LowPassStateRight;

				resampledData[dataIndex] = inLeft - pData->LowPassStateLeft;
				resampledData[dataIndex + 1] = inRight - pData->LowPassStateRight;
			}

			std::vector<std::int8_t> packet1(64, 0);
			for (std::int32_t i = 0; i < 32; ++i)
			{
				const std::int32_t dataIndex = i * 2;

				float leftSample = resampledData[dataIndex];
				float rightSample = resampledData[dataIndex + 1];

				std::int8_t leftInt8 = static_cast<std::int8_t>(std::clamp(static_cast<int>(std::round(leftSample * 127.0f)), -128, 127));
				std::int8_t rightInt8 = static_cast<std::int8_t>(std::clamp(static_cast<int>(std::round(rightSample * 127.0f)), -128, 127));

				packet1[dataIndex] = leftInt8;
				packet1[dataIndex + 1] = rightInt8;
			}

			std::vector<std::int8_t> packet2(64, 0);
			for (std::int32_t i = 0; i < 32; ++i)
			{
				const std::int32_t dataIndex = (i + 32) * 2;

				float leftSample = resampledData[dataIndex];
				float rightSample = resampledData[dataIndex + 1];

				std::int8_t leftInt8 = static_cast<std::int8_t>(std::clamp(static_cast<int>(std::round(leftSample * 127.0f)), -128, 127));
				std::int8_t rightInt8 = static_cast<std::int8_t>(std::clamp(static_cast<int>(std::round(rightSample * 127.0f)), -128, 127));

				const std::int32_t packetIndex = i * 2;
				packet2[packetIndex] = leftInt8;
				packet2[packetIndex + 1] = rightInt8;
			}

			std::vector<std::uint8_t> packet1Unsigned(packet1.begin(), packet1.end());
			std::vector<std::uint8_t> packet2Unsigned(packet2.begin(), packet2.end());

			pData->btPacketQueue.Push(packet1Unsigned);
			pData->btPacketQueue.Push(packet2Unsigned);
		}
	}

	pData->framesPlayed += framesRead;
}

void ConsumeHapticsQueue(IGamepadAudioHaptics* AudioHaptics, AudioCallbackData& callbackData, bool IsWireless = false)
{

	if (IsWireless)
	{
		std::vector<std::uint8_t> packet;
		while (callbackData.btPacketQueue.Pop(packet))
		{
			AudioHaptics->AudioHapticUpdate(packet);
		}
	}
	else
	{
		std::lock_guard<std::mutex> lock(callbackData.usbMutex);
		std::vector<std::int16_t> allSamples;
		allSamples.reserve(2048 * 2);

		std::vector<std::int16_t> stereoSample;
		while (callbackData.usbSampleQueue.Pop(stereoSample))
		{
			if (stereoSample.size() >= 2)
			{
				allSamples.push_back(stereoSample[0]);
				allSamples.push_back(stereoSample[1]);
			}
		}

		if (!allSamples.empty())
		{
			AudioHaptics->AudioHapticUpdate(allSamples);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}
}

ma_device g_AudioDevice;
bool g_AudioDeviceInitialized = false;
AudioCallbackData g_AudioCallbackData;

void AudioLoop()
{
	std::cout << "[AppDLL] Audio Loop Started." << std::endl;

	ISonyGamepad* Gamepad = g_Registry->GetLibrary(0);
	while (g_Running)
	{
		if (Gamepad && Gamepad->IsConnected())
		{
			IGamepadAudioHaptics* AudioHaptics = Gamepad->GetIGamepadHaptics();
			if (AudioHaptics)
			{
				if (!g_AudioDeviceInitialized)
				{
					if (g_AudioDeviceInitialized)
					{
						ma_device_uninit(&g_AudioDevice);
						g_AudioDeviceInitialized = false;

						{
							std::lock_guard<std::mutex> lock(g_AudioCallbackData.btAccumulatorMutex);
							g_AudioCallbackData.btAccumulator.clear();
						}
						std::vector<uint8_t> dummy;
						while (g_AudioCallbackData.btPacketQueue.Pop(dummy));
						std::vector<int16_t> dummy2;
						while (g_AudioCallbackData.usbSampleQueue.Pop(dummy2));
					}

					static bool bIsWireless = Gamepad->GetConnectionType() == EDSDeviceConnection::Bluetooth;
					std::cout << "[AppDLL] Initializing Audio Loopback for Haptics (" << (bIsWireless ? "Bluetooth" : "USB") << ")..." << std::endl;

					FDeviceContext* Context = Gamepad->GetMutableDeviceContext();
					if (!bIsWireless && Context)
					{
						if (!Context->AudioContext || !Context->AudioContext->IsValid())
						{
							IPlatformHardwareInfo::Get().InitializeAudioDevice(Context);
						}
					}

					g_AudioCallbackData.bIsSystemAudio = true;
					g_AudioCallbackData.bIsWireless = bIsWireless;
					g_AudioCallbackData.bFinished = false;

					ma_device_config deviceConfig = ma_device_config_init(ma_device_type_loopback);
					deviceConfig.capture.format = ma_format_f32;
					deviceConfig.capture.channels = 2;
					deviceConfig.sampleRate = 48000;
					deviceConfig.dataCallback = AudioDataCallback;
					deviceConfig.pUserData = &g_AudioCallbackData;
					deviceConfig.wasapi.loopbackProcessID = 0;

					ma_result result = ma_device_init(nullptr, &deviceConfig, &g_AudioDevice);
					if (result == MA_SUCCESS)
					{
						if (ma_device_start(&g_AudioDevice) == MA_SUCCESS)
						{
							g_AudioDeviceInitialized = true;
							std::cout << "[AppDLL] Audio Loopback Started." << std::endl;
						}
						else
						{
							ma_device_uninit(&g_AudioDevice);
							std::cerr << "[AppDLL] Failed to start audio device." << std::endl;
						}
					}
					else
					{
						std::cerr << "[AppDLL] Failed to initialize audio device (Error: " << result << ")." << std::endl;
					}
				}
				ConsumeHapticsQueue(AudioHaptics, g_AudioCallbackData, Gamepad->GetConnectionType() == EDSDeviceConnection::Bluetooth);
			}
		}
		else
		{
			if (g_AudioDeviceInitialized)
			{
				ma_device_uninit(&g_AudioDevice);
				g_AudioDeviceInitialized = false;
				std::cout << "[AppDLL] Audio Loopback Stopped (Controller Disconnected)." << std::endl;
			}
		}

	}

	if (g_AudioDeviceInitialized)
	{
		ma_device_uninit(&g_AudioDevice);
		g_AudioDeviceInitialized = false;
	}

	std::cout << "[AppDLL] Audio Loop Stopped." << std::endl;
}

void InputLoop()
{
	std::cout << "[AppDLL] Input Loop Started." << std::endl;

	uint64_t FrameCounter = 0;
	while (g_Running)
	{
		float DeltaTime = 0.0166f;
		ISonyGamepad* Gamepad = g_Registry->GetLibrary(0);
		if (!Gamepad)
		{
			g_Registry->PlugAndPlay(DeltaTime);
		}


		if (Gamepad)
		{
			if (FrameCounter % 100 == 0)
			{
				Gamepad->DualSenseSettings(1, 1, 1, 0, 30, 0xFC, 0x00, 0x00);
				auto Trigger = Gamepad->GetIGamepadTrigger();
				if (Trigger)
				{
					Trigger->SetResistance(0, 0xff, EDSGamepadHand::AnyHand);
				}
				Gamepad->SetLightbar({200, 160, 80});
				Gamepad->UpdateOutput();
			}
		}

		if (Gamepad && Gamepad->GetConnectionType() == EDSDeviceConnection::Bluetooth)
		{
			Gamepad->UpdateInput(DeltaTime);
		}

		if (Gamepad && Gamepad->IsConnected())
		{
			FDeviceContext* DeviceContext = Gamepad->GetMutableDeviceContext();
			if (DeviceContext)
			{
				FInputContext* CurrentState = DeviceContext->GetInputState();
				if (CurrentState)
				{
#ifdef USE_VIGEM
					if (g_ViGEmAdapter && Gamepad->GetConnectionType() == EDSDeviceConnection::Bluetooth)
					{
						g_ViGEmAdapter->Update(*CurrentState);
					}
#endif
				}
			}

		}
		else
		{
			static int NullCounter = 0;
			if (++NullCounter % 60 == 0)
			{
				std::cout << "[AppDLL] Gamepad Object is NULL for ID " << g_Registry->Policy.deviceId << std::endl;
			}
		}

		FrameCounter++;
	}

	std::cout << "[AppDLL] Input Loop Stopped." << std::endl;
}

void CreateConsole()
{
	if (GetConsoleWindow() != NULL)
	{
		return;
	}
	if (AllocConsole())
	{
		FILE* fp;
		freopen_s(&fp, "CONOUT$", "w", stdout);
		freopen_s(&fp, "CONOUT$", "w", stderr);
		freopen_s(&fp, "CONIN$", "r", stdin);

		setvbuf(stdout, NULL, _IONBF, 0);
		setvbuf(stderr, NULL, _IONBF, 0);

		std::cout << "[Console] Debug Console Attached!" << std::endl;
	}
}

void StartServiceThread()
{
	if (g_ServiceInitialized.exchange(true))
		return;

	CreateConsole();

	InitMod();

	std::cout << "[AppDLL] Service Thread Starting..." << std::endl;
	std::cout.flush();

	g_LastInputState = FInputContext();

	std::cout << "[System] Initializing Hardware Layer..." << std::endl;
	std::cout.flush();

	auto HardwareImpl = std::make_unique<TestHardwareInfo>();
	IPlatformHardwareInfo::SetInstance(std::move(HardwareImpl));

	g_Registry = std::make_unique<TestDeviceRegistry>();
	g_Registry->Policy.deviceId = 0;

	std::cout << "[System] Requesting Immediate Detection..." << std::endl;
	std::cout.flush();

	while (true)
	{
		ISonyGamepad* Gamepad = g_Registry->GetLibrary(0);
		if (Gamepad)
		{
			Gamepad->DualSenseSettings(1, 1, 1, 0, 30, 0xFC, 0x00, 0x00);
			auto Trigger = Gamepad->GetIGamepadTrigger();
			if (Trigger)
			{
				Trigger->SetResistance(0, 0xff, EDSGamepadHand::AnyHand);
			}
			Gamepad->SetLightbar({200, 160, 80});

			break;
		}

		g_Registry->PlugAndPlay(2.0f);
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	ISonyGamepad* Gamepad = g_Registry->GetLibrary(0);

#ifdef USE_VIGEM
	if (Gamepad)
	{
		std::cout << "[System] Initializing ViGEm Adapter (Bluetooth Mode)..." << std::endl;
		g_ViGEmAdapter = std::make_unique<ViGEmAdapter>();
		if (!g_ViGEmAdapter->Initialize())
		{
			std::cerr << "[System] ViGEm Adapter failed to initialize. Xbox Emulation will not be available." << std::endl;
		}
	}
#endif


	std::cout << "[System] Waiting for controller connection via USB/BT..." << std::endl;
	std::cout.flush();

	if (Gamepad)
	{
		Gamepad->UpdateOutput();
	}

	g_Running = true;

	std::cout << "[AppDLL] Gamepad Service Started." << std::endl;
	std::cout.flush();

	g_AudioThread = std::thread(AudioLoop);

	InputLoop();

	if (g_AudioThread.joinable())
	{
		g_AudioThread.join();
	}

	std::cout << "[AppDLL] Gamepad Service Stopped." << std::endl;
	std::cout.flush();
	g_ServiceInitialized = false;
}

extern "C"
{

__declspec(dllexport) void StartGamepadService()
{
	if (g_Running || g_ServiceInitialized)
	{
		return;
	}

	if (g_ServiceThread.joinable())
	{
		g_ServiceThread.detach();
	}

	g_ServiceThread = std::thread(StartServiceThread);
}

__declspec(dllexport) void StopGamepadService()
{
	g_Running = false;
}

}

#ifndef BUILDING_PROXY_DLL
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH: StartGamepadService();
			break;

		case DLL_PROCESS_DETACH:
			g_Running = false;

#ifdef USE_VIGEM
			if (g_ViGEmAdapter)
			{
				g_ViGEmAdapter->Shutdown();
				g_ViGEmAdapter.reset();
			}
#endif

			if (g_Registry)
			{
				g_Registry.reset();
			}

			if (lpReserved == nullptr)
			{
				if (g_AudioThread.joinable())
					g_AudioThread.detach();
				if (g_ServiceThread.joinable())
					g_ServiceThread.detach();
			}

			g_ServiceInitialized = false;
			break;
	}
	return TRUE;
}
#endif
