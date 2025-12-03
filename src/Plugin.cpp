// src/Plugin.cpp
#include <windows.h>
#include <iostream>
#include <string>
#include <exception>
#include <cstdint>

// RakHook + RakNet (AsiPlugin подтягивает эти зависимости через CMake/FetchContent)
#include "RakHook/RakHook.hpp"
#include "RakNet/BitStream.h"

// ----------------- Ваша логика парсинга пакета (порт из Lua) -----------------
void ProcessPacket(unsigned char id, RakNet::BitStream &bs)
{
    static bool isActive = true;
    if (!isActive) return;

    if (id != 215) return;

    // Lua: raknetBitStreamReadInt8(bs)
    int8_t tmp1 = 0;
    if (!bs.Read(tmp1)) return;

    // style (int16), types (int32)
    int16_t style = 0;
    int32_t types = 0;
    if (!bs.Read(style)) return;
    if (!bs.Read(types)) return; // types не используется, но читаем для сдвига

    if (style != 2) return;

    // внутри style==2: int8, len1+str1, len2+str2
    int8_t tmp2 = 0;
    if (!bs.Read(tmp2)) return;

    int32_t len1 = 0;
    if (!bs.Read(len1)) return;
    std::string str1;
    if (len1 > 0) {
        try {
            str1.resize(len1);
        } catch (...) { return; }
        if (!bs.Read(&str1[0], len1 * static_cast<int>(sizeof(char)))) return;
    }

    int32_t len2 = 0;
    if (!bs.Read(len2)) return;
    std::string str2;
    if (len2 > 0) {
        try {
            str2.resize(len2);
        } catch (...) { return; }
        if (!bs.Read(&str2[0], len2 * static_cast<int>(sizeof(char)))) return;
    }

    // Поведение вывода как в Lua: если есть str1 — печатаем str1 (и str2 если есть), иначе печатаем str2 если есть.
    if (!str1.empty()) {
        if (!str2.empty()) {
            OutputDebugStringA((str1 + "\n" + str2 + "\n").c_str());
        } else {
            OutputDebugStringA((str1 + "\n").c_str());
        }
    } else if (!str2.empty()) {
        OutputDebugStringA((str2 + "\n").c_str());
    }
}
// ----------------------------------------------------------------------------

// Обёртка, вызываемая в лямбде подписки
void OnReceivePacket_Impl(const unsigned char* data, unsigned int length)
{
    if (!data || length == 0) return;

    unsigned char id = data[0];

    if (length <= 1) return;

    // RakNet::BitStream принимает void* — снимаем const-регистрацию, но не модифицируем данные.
    RakNet::BitStream bs(const_cast<unsigned char*>(data + 1), length - 1, false);

    try {
        ProcessPacket(id, bs);
    } catch (const std::exception& e) {
        OutputDebugStringA("[OnReceivePacket_Impl] exception\n");
    } catch (...) {
        OutputDebugStringA("[OnReceivePacket_Impl] unknown exception\n");
    }
}

// DllMain: инициализация / деинициализация RakHook
BOOL WINAPI DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        // Инициализируем RakHook (AsiPlugin/CMake должен уже подтянуть rakhook)
        if (!rakhook::initialize()) {
            MessageBoxA(NULL, "RakHook initialization failed", "AsiPlugin", MB_OK | MB_ICONERROR);
            return FALSE;
        }

        // Подписываемся на получение пакетов. Лямбда подходит под требуемый std::function типа rakhook::receive_t.
        rakhook::on_receive_packet += [](const unsigned char* data, unsigned int length) {
            OnReceivePacket_Impl(data, length);
        };

        OutputDebugStringA("[AsiPlugin] RC CEF (by CanVas Dev) activated\n");
        break;

    case DLL_PROCESS_DETACH:
        // Отключаем RakHook
        rakhook::destroy();
        OutputDebugStringA("[AsiPlugin] RC CEF detached\n");
        break;
    }
    return TRUE;
}
