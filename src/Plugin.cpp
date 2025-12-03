// src/Plugin.cpp
// Адаптация для lirikasamp/AsiPlugin
// - не требует RakHook/Packet.h (forward-declare Packet)
// - регистрирует callback через std::function<rakhook::receive_t>
// - не содержит DllMain (main.cpp ваш уже содержит DllMain)

#include <iostream>
#include <string>
#include <exception>
#include <functional>
#include <cstdint>

#include "RakHook/RakHook.hpp"
#include "RakNet/BitStream.h"

// Forward-declaration: реальный заголовок Packet.h может отсутствовать в include-path,
// поэтому не включаем его и не завязываемся на его определение здесь.
struct Packet;

// ----------------- ПАРСЕР (перенос логики из Lua) -----------------
static void ProcessPacket(unsigned char id, RakNet::BitStream &bs)
{
    static bool isActive = true;
    if (!isActive) return;

    if (id != 215) return; // интересует лишь пакет 215

    // Lua: raknetBitStreamReadInt8(bs)
    int8_t tmp1 = 0;
    if (!bs.Read(tmp1)) return;

    // style (int16), types (int32)
    int16_t style = 0;
    int32_t types = 0;
    if (!bs.Read(style)) return;
    if (!bs.Read(types)) return; // types читаем для сдвига, не используем

    if (style != 2) return;

    // внутри style==2: int8, len1+str1, len2+str2
    int8_t tmp2 = 0;
    if (!bs.Read(tmp2)) return;

    int32_t len1 = 0;
    if (!bs.Read(len1)) return;
    std::string str1;
    if (len1 > 0) {
        // защита от абсурда
        if (len1 < 0 || len1 > 16 * 1024 * 1024) return;
        str1.resize(len1);
        if (!bs.Read(&str1[0], static_cast<int>(len1 * sizeof(char)))) return;
    }

    int32_t len2 = 0;
    if (!bs.Read(len2)) return;
    std::string str2;
    if (len2 > 0) {
        if (len2 < 0 || len2 > 16 * 1024 * 1024) return;
        str2.resize(len2);
        if (!bs.Read(&str2[0], static_cast<int>(len2 * sizeof(char)))) return;
    }

    // Поведение вывода как в Lua: если есть str1 — печатаем str1 (и str2 если есть),
    // иначе печатаем str2 если есть.
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
// ------------------------------------------------------------------


// ----------------- обработчик, совместимый с rakhook::receive_t -----------------
// Используем forward-declared Packet*, но для доступа к data/length выполняем приведение
// к proxy-структуре с ожидаемыми полями.
// В большинстве сборок RakNet Packet имеет первые публичные поля unsigned char* data; unsigned int length;
static bool OnReceivePacket(Packet*& p)
{
    if (!p) return true;

    // proxy-структура: ожидаем хотя бы первые 2 поля в Packet.
    struct PacketProxy {
        unsigned char* data;
        unsigned int   length;
        // далее могут быть другие поля, но нам они не нужны
    };

    PacketProxy* proxy = reinterpret_cast<PacketProxy*>(p);
    if (!proxy || !proxy->data || proxy->length == 0) return true;

    // читаем id
    unsigned char id = proxy->data[0];
    if (proxy->length <= 1) return true;

    // создаём BitStream над payload (смещаем на 1 байт, чтобы пропустить id)
    RakNet::BitStream bs(proxy->data + 1, proxy->length - 1, false);

    try {
        ProcessPacket(id, bs);
    } catch (const std::exception& e) {
        OutputDebugStringA("[OnReceivePacket] exception\n");
    } catch (...) {
        OutputDebugStringA("[OnReceivePacket] unknown exception\n");
    }

    // вернуть true, чтобы не блокировать дальнейшую/оригинальную обработку пакета
    return true;
}
// ---------------------------------------------------------------------------


// ----------------- регистрация обработчика (статический объект) -----------------
// Создаём std::function<rakhook::receive_t> из свободной функции OnReceivePacket.
// Это обходит проблемы MSVC при попытке неявного приведения лямбд/перегрузок.
struct PacketHookRegistrar {
    PacketHookRegistrar() {
        try {
            // rakhook::receive_t обычно имеет сигнатуру bool(Packet*&)
            std::function<rakhook::receive_t> fn(OnReceivePacket);
            rakhook::on_receive_packet += fn;
            OutputDebugStringA("[Plugin] subscribed to rakhook::on_receive_packet\n");
        } catch (...) {
            OutputDebugStringA("[Plugin] failed to subscribe to rakhook::on_receive_packet\n");
        }
    }
    ~PacketHookRegistrar() {
        // в большинстве реализаций rakhook::on_receive_packet поддерживает operator-=,
        // но отсутствие конкретной функции-идентификатора затрудняет удаление.
        // Оставляем очистку RakHook на rakhook::destroy() (обычно вызывается в DllMain/other).
    }
};

// статический экземпляр автоматом запускает регистрацию при загрузке модуля
static PacketHookRegistrar s_packetHookRegistrar;
