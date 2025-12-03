// plugin.cpp — заменяет шаблонный файл в AsiPlugin

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

// Подключаем RakHook / SAMP‑API хедеры — путь может отличаться
#include "RakHook/RakHook.hpp"
#include "RakNet/BitStream.h"

// Функция для разбора пакетов — перенесена из твоего Lua / C++‑версии
void ProcessPacket(int id, RakNet::BitStream &bs) {
    if (id == 215) {
        int8_t dummy1;
        bs.Read(dummy1);

        int16_t style;
        bs.Read(style);
        int32_t types;
        bs.Read(types);

        if (style == 2) {
            int8_t dummy2;
            bs.Read(dummy2);

            int32_t len1;
            bs.Read(len1);
            std::string str1;
            if (len1 > 0) {
                str1.resize(len1);
                bs.Read(&str1[0], len1 * sizeof(char));
            }

            int32_t len2;
            bs.Read(len2);
            std::string str2;
            if (len2 > 0) {
                str2.resize(len2);
                bs.Read(&str2[0], len2 * sizeof(char));
            }

            if (!str1.empty()) {
                if (!str2.empty()) std::cout << str1 << "\n" << str2 << std::endl;
                else std::cout << str1 << std::endl;
            } else if (!str2.empty()) {
                std::cout << str2 << std::endl;
            }
        }
    }
}

// Callback RakHook
void OnReceivePacket_Callback(RakNet::Packet *packet) {
    unsigned char id = packet->data[0];
    RakNet::BitStream bs(packet->data + 1, packet->length - 1, false);
    ProcessPacket(id, bs);
}

// Entry point DLL
BOOL WINAPI DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        // Инициализация RakHook
        if (!rakhook::initialize()) {
            MessageBoxA(NULL, "RakHook initialize failed", "Error", MB_OK | MB_ICONERROR);
            return FALSE;
        }
        // Регистрация callback
        rakhook::on_receive_packet += OnReceivePacket_Callback;

        MessageBoxA(NULL, "RC CEF (ASI) Activated", "Info", MB_OK);
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        rakhook::destroy();
    }
    return TRUE;
}
