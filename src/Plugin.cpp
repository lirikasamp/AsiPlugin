// src/Plugin.cpp
#include <iostream>
#include <string>
#include <exception>
#include "RakHook/RakHook.hpp"
#include "RakNet/BitStream.h"
#include "RakHook/Packet.h"  // теперь реальный заголовок Packet

// ----------------- Логика разбора пакета -----------------
static void ProcessPacket(unsigned char id, RakNet::BitStream &bs)
{
    static bool isActive = true;
    if (!isActive) return;
    if (id != 215) return;

    int8_t tmp1 = 0;
    if (!bs.Read(tmp1)) return;

    int16_t style = 0;
    int32_t types = 0;
    if (!bs.Read(style)) return;
    if (!bs.Read(types)) return;

    if (style != 2) return;

    int8_t tmp2 = 0;
    if (!bs.Read(tmp2)) return;

    int32_t len1 = 0;
    if (!bs.Read(len1)) return;
    std::string str1;
    if (len1 > 0) {
        str1.resize(len1);
        if (!bs.Read(&str1[0], len1 * sizeof(char))) return;
    }

    int32_t len2 = 0;
    if (!bs.Read(len2)) return;
    std::string str2;
    if (len2 > 0) {
        str2.resize(len2);
        if (!bs.Read(&str2[0], len2 * sizeof(char))) return;
    }

    if (!str1.empty()) {
        if (!str2.empty())
            OutputDebugStringA((str1 + "\n" + str2 + "\n").c_str());
        else
            OutputDebugStringA((str1 + "\n").c_str());
    } else if (!str2.empty()) {
        OutputDebugStringA((str2 + "\n").c_str());
    }
}

// ----------------- Callback -----------------
static bool OnReceivePacket(Packet*& packet)
{
    if (!packet || !packet->data || packet->length == 0) return true;

    unsigned char id = packet->data[0];
    if (packet->length <= 1) return true;

    RakNet::BitStream bs(packet->data + 1, packet->length - 1, false);

    try {
        ProcessPacket(id, bs);
    } catch (...) {
        OutputDebugStringA("[OnReceivePacket] exception\n");
    }

    return true; // пропустить пакет дальше
}

// ----------------- Регистрация -----------------
struct PacketHookRegister
{
    PacketHookRegister() {
        rakhook::on_receive_packet += std::function<rakhook::receive_t>(OnReceivePacket);
        OutputDebugStringA("[Plugin] subscribed to rakhook::on_receive_packet\n");
    }
};

// Статический объект для автоматической регистрации
static PacketHookRegister s_packetHookRegister;
