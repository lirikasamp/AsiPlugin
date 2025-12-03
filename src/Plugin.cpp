// src/Plugin.cpp
// Полностью готовый к использованию файл для lirikasamp/AsiPlugin
// - НЕ включает RakHook/Packet.h (его у вас нет в include-path)
// - использует forward-declare Packet и proxy-структуру для доступа к data/length
// - регистрирует обработчик через std::function, созданную из лямбды,
//   чтобы MSVC корректно преобразовал типы (устраняет C2665/C2440)

#include <iostream>
#include <string>
#include <exception>
#include <functional>
#include <cstdint>

#include "RakHook/RakHook.hpp"
#include "RakNet/BitStream.h"

// Forward-declare: реального заголовка Packet.h в include-path у вас нет,
// поэтому не подключаем его и не зависим от точного определения.
struct Packet;

// ----------------- ПАРСЕР (перенос логики из Lua) -----------------
static void ProcessPacket(unsigned char id, RakNet::BitStream &bs)
{
    static bool isActive = true;
    if (!isActive) return;

    if (id != 215) return; // нас интересуют только пакеты с id 215

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
        if (len1 < 0 || len1 > 16 * 1024 * 1024) return; // защита от абсурда
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
// к proxy-структуре с ожидаемыми полями (распространённый паттерн в моддинге SA-MP).
static bool OnReceivePacket_Impl(Packet*& p)
{
    if (!p) return true;

    // proxy-структура: ожидаем первые публичные поля Packet: unsigned char* data; unsigned int length;
    struct PacketProxy {
        unsigned char* data;
        unsigned int length;
    };

    PacketProxy* proxy = reinterpret_cast<PacketProxy*>(p);
    if (!proxy || !proxy->data || proxy->length == 0) return true;

    unsigned char id = proxy->data[0];
    if (proxy->length <= 1) return true;

    // создаём BitStream над payload (смещаем на 1 байт)
    RakNet::BitStream bs(proxy->data + 1, proxy->length - 1, false);

    try {
        ProcessPacket(id, bs);
    } catch (const std::exception& e) {
        OutputDebugStringA("[OnReceivePacket_Impl] exception\n");
    } catch (...) {
        OutputDebugStringA("[OnReceivePacket_Impl] unknown exception\n");
    }

    // вернуть true, чтобы не блокировать дальнейшую обработку пакета в оригинальном коде
    return true;
}
// ----------------------------------------------------------------------------


// ----------------- Регистрация обработчика -----------------
// Создаём std::function<rakhook::receive_t> явно из ЛЯМБДЫ с требуемой сигнатурой.
// Это гарантирует, что MSVC корректно сконструирует std::function и operator+= сработает.

struct PacketHookRegistrar {
    PacketHookRegistrar() {
        try {
            // Создаём лямбду с точно той же сигнатурой, что и rakhook::receive_t.
            // В ранних/распространённых версиях RakHook это: bool(Packet*&).
            auto lambda = [](Packet*& p) -> bool {
                return OnReceivePacket_Impl(p);
            };

            // Явно создаём std::function<rakhook::receive_t> из лямбды.
            std::function<rakhook::receive_t> fn(lambda);

            // Регистрируем
            rakhook::on_receive_packet += fn;

            OutputDebugStringA("[Plugin] subscribed to rakhook::on_receive_packet\n");
        } catch (...) {
            OutputDebugStringA("[Plugin] failed to subscribe to rakhook::on_receive_packet\n");
        }
    }

    ~PacketHookRegistrar() {
        // Не делаем ничего: штатная очистка rakhook::destroy() в main/другом месте освободит ресурсы.
    }
};

// Инстанцируем статический объект — регистрация произойдёт при загрузке модуля (без DllMain).
static PacketHookRegistrar s_packetHookRegistrar;
