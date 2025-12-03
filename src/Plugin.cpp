// Plugin.cpp — замените им ваш текущий файл в AsiPlugin
#include <windows.h>
#include <iostream>
#include <string>

// RakHook + RakNet (зависимости, которые уже используются в шаблоне AsiPlugin)
#include "RakHook/RakHook.hpp"
#include "RakNet/BitStream.h"
#include "RakNet/Packet.h"    // <--- Packet в глобальном неймспейсе

// Если хотите вывод в лог SA-MP, вместо std::cout используйте соответствующие функции.
// Здесь для простоты — std::cout.

// Разбор пакета (ваша логика, port'нутая из Lua)
void ProcessPacket(unsigned char id, RakNet::BitStream &bs)
{
    // флаг активности можно вынести наружу, если нужно
    static bool isActive = true;
    if (!isActive) return;

    if (id == 215) {
        int8_t tmp1 = 0;
        // чтение первого байта (в Lua был raknetBitStreamReadInt8(bs))
        bs.Read(tmp1);

        int16_t style = 0;
        int32_t types = 0;
        bs.Read(style);
        bs.Read(types); // types не используется, но читаем для синхронизации

        if (style == 2) {
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

            if (!str1.empty()) {
                if (!str2.empty())
                    std::cout << str1 << std::endl << str2 << std::endl;
                else
                    std::cout << str1 << std::endl;
            } else if (!str2.empty()) {
                std::cout << str2 << std::endl;
            }
        }
    }
}

// Небольшая обёртка — безопасно вызывает ProcessPacket
void OnReceivePacket_Impl(::Packet* packet)
{
    if (!packet) return;
    // первый байт — id (raw packet data)
    unsigned char id = packet->data[0];

    // создаём BitStream над payload (смещение на 1 байт)
    // конструктор BitStream(void* bitData, unsigned int bitLengthInBytes, bool copy)
    RakNet::BitStream bs(packet->data + 1, packet->length - 1, false);

    // вызываем разбор
    try {
        ProcessPacket(id, bs);
    } catch (const std::exception& e) {
        std::cerr << "[OnReceivePacket_Impl] exception: " << e.what() << std::endl;
    }
}

// DllMain: инициализация / очистка RakHook
BOOL WINAPI DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        // Инициализируем RakHook (вернёт true при успехе)
        if (!rakhook::initialize()) {
            MessageBoxA(NULL, "RakHook initialization failed", "AsiPlugin", MB_OK | MB_ICONERROR);
            return FALSE;
        }

        // Регистрируем callback как лямбду — так гарантированно корректно преобразуется в std::function
        rakhook::on_receive_packet += [](::Packet* p) {
            OnReceivePacket_Impl(p);
        };

        // Информируем о старте (можно удалить)
        OutputDebugStringA("[AsiPlugin] RC CEF (by CanVas Dev) activated\n");
        break;

    case DLL_PROCESS_DETACH:
        // Удаляем обработчики (RakHook::destroy снимет подписки), затем освобождаем ресурсы
        rakhook::destroy();
        OutputDebugStringA("[AsiPlugin] RC CEF detached\n");
        break;
    }
    return TRUE;
}
