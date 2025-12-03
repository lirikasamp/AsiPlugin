// src/Plugin.cpp
#include <windows.h>
#include <iostream>
#include <string>
#include <exception>

// RakHook + RakNet includes (AsiPlugin поставляет RakHook как зависимость через CMake/FetchContent)
#include "RakHook/RakHook.hpp"
#include "RakNet/BitStream.h"
#include "RakNet/Packet.h" // Packet находится в глобальном неймспейсе

// ---- Ваша логика разбора пакета (переписанная из Lua) ----
void ProcessPacket(unsigned char id, RakNet::BitStream &bs)
{
    static bool isActive = true;
    if (!isActive) return;

    if (id != 215) return;

    // Lua: raknetBitStreamReadInt8(bs)
    int8_t tmp1 = 0;
    bs.Read(tmp1);

    // style (int16), types (int32)
    int16_t style = 0;
    int32_t types = 0;
    bs.Read(style);
    bs.Read(types); // types не используется, но читаем чтобы сохранить сдвиг

    if (style != 2) return;

    // внутри style==2: int8, len1+str1, len2+str2
    int8_t tmp2 = 0;
    bs.Read(tmp2);

    int32_t len1 = 0;
    bs.Read(len1);
    std::string str1;
    if (len1 > 0) {
        str1.resize(len1);
        bs.Read(&str1[0], len1 * sizeof(char));
    }

    int32_t len2 = 0;
    bs.Read(len2);
    std::string str2;
    if (len2 > 0) {
        str2.resize(len2);
        bs.Read(&str2[0], len2 * sizeof(char));
    }

    // Поведение вывода как в Lua: если есть str1 — печатаем str1 (и str2 если есть),
    // иначе печатаем str2 если она есть.
    if (!str1.empty()) {
        if (!str2.empty()) {
            std::cout << str1 << '\n' << str2 << std::endl;
        } else {
            std::cout << str1 << std::endl;
        }
    } else if (!str2.empty()) {
        std::cout << str2 << std::endl;
    }
}
// -----------------------------------------------------------

// Обёртка: вызывается из лямбды подписки
void OnReceivePacket_Impl(::Packet* packet)
{
    if (!packet) return;
    // packet->data[0] — ID пакета
    unsigned char id = packet->data[0];

    // создаём BitStream над payload (пропускаем первый байт — id)
    if (packet->length <= 1) return;
    RakNet::BitStream bs(packet->data + 1, packet->length - 1, false);

    try {
        ProcessPacket(id, bs);
    } catch (const std::exception& e) {
        std::cerr << "[OnReceivePacket_Impl] exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[OnReceivePacket_Impl] unknown exception" << std::endl;
    }
}

// DllMain: инициализация / деинициализация RakHook
BOOL WINAPI DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        // Инициализируем RakHook (AsiPlugin / CMake уже подтянули rakhook)
        if (!rakhook::initialize()) {
            MessageBoxA(NULL, "RakHook initialization failed", "AsiPlugin", MB_OK | MB_ICONERROR);
            return FALSE;
        }

        // Регистрируем callback как лямбду — корректно конвертируется в std::function
        rakhook::on_receive_packet += [](::Packet* p) {
            OnReceivePacket_Impl(p);
        };

        OutputDebugStringA("[AsiPlugin] RC CEF (by CanVas Dev) activated\n");
        break;

    case DLL_PROCESS_DETACH:
        // Корректно закрываем RakHook
        rakhook::destroy();
        OutputDebugStringA("[AsiPlugin] RC CEF detached\n");
        break;
    }
    return TRUE;
}
