// src/Plugin.cpp
// Адаптирован для lirikasamp/AsiPlugin (нет DllMain, подписка через статический инициализатор)

#include <iostream>
#include <string>
#include <exception>
#include <cstdint>
#include <functional>

// RakHook + RakNet BitStream (AsiPlugin подтягивает rakhook через FetchContent)
#include "RakHook/RakHook.hpp"
#include "RakNet/BitStream.h"

// В репозитории нет RakNet/Packet.h, но RakHook использует тип Packet* в on_receive_packet.
// Делаем forward declaration, чтобы использовать Packet*& без include'а.
struct Packet;

// ----------------- Ваша логика парсинга пакета (порт из Lua) -----------------
static void ProcessPacket(unsigned char id, RakNet::BitStream &bs)
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
    if (!bs.Read(types)) return;

    if (style != 2) return;

    // внутри style==2: int8, len1+str1, len2+str2
    int8_t tmp2 = 0;
    if (!bs.Read(tmp2)) return;

    int32_t len1 = 0;
    if (!bs.Read(len1)) return;
    std::string str1;
    if (len1 > 0) {
        // защитная проверка на разумный размер (необязательно, но полезно)
        if (len1 < 0 || len1 > 10 * 1024 * 1024) return;
        str1.resize(len1);
        if (!bs.Read(&str1[0], len1 * static_cast<int>(sizeof(char)))) return;
    }

    int32_t len2 = 0;
    if (!bs.Read(len2)) return;
    std::string str2;
    if (len2 > 0) {
        if (len2 < 0 || len2 > 10 * 1024 * 1024) return;
        str2.resize(len2);
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

// Функция-обработчик, совместимая с сигнатурой rakhook::receive_t (Packet *& p) -> bool
static bool OnReceivePacket_Impl(Packet*& p)
{
    // Проверки безопасности
    if (!p) return true; // возвращаем true, чтобы не блокировать дальнейшую обработку (согласно поведению rakhook)

    // В Packet ожидаем поля: data (unsigned char*), length (unsigned int)
    // Чтобы не зависеть от RakNet/Packet.h (его нет у вас), используем приведение через известные офсеты:
    // Но чаще всего Packet имеет поля data и length; большинство сборок RakHook/PR используют именно это.
    // Попробуем обращаться напрямую — если структура отличается, придёт ошибка времени выполнения.
    // Для билд/компиляции forward-declaration достаточно; at runtime поле data/length должны существовать.

    // Приводим p к структуре с нужными полями (не определяя её здесь) — используем reinterpret_cast.
    // ВАЖНО: Это небезопасно, но в привычных сборках RakNet Packet содержит public unsigned char *data; unsigned int length; 
    // Поэтому такое обращение обычно работает в проектах SA-MP + RakHook.

    // Получаем указатель на данные и длину через reinterpret_cast доступа к известным офсетам.
    // Чтобы не гадать с layout, используем C++-стиль: предполагаем, что компилятор и RakNet совместимы.
    // Ниже — "duck-typed" доступ: объявим временный proxy-struct с ожидаемыми полями и приведём p к нему.

    struct PacketProxy {
        unsigned char* data;
        unsigned int length;
        // дальше могут идти другие поля, но они нам не нужны
    };

    PacketProxy* proxy = reinterpret_cast<PacketProxy*>(p);
    if (!proxy->data || proxy->length == 0) return true;

    unsigned char id = proxy->data[0];
    if (proxy->length <= 1) return true;

    // Создаём BitStream поверх payload (смещаем на 1 байт)
    RakNet::BitStream bs(proxy->data + 1, proxy->length - 1, false);

    try {
        ProcessPacket(id, bs);
    } catch (const std::exception& e) {
        OutputDebugStringA("[OnReceivePacket_Impl] exception\n");
    } catch (...) {
        OutputDebugStringA("[OnReceivePacket_Impl] unknown exception\n");
    }

    return true; // вернуть true — обычно означает "пустить дальше оригинальную обработку"
}

// ----------------- Регистрация колбека (статический инициализатор) -----------------
// Мы не трогаем DllMain — main.cpp вашего репо уже его содержит.
// Вместо этого подписываем колбек в момент инициализации модулей — через статический объект.
// Явно создаём std::function<rakhook::receive_t> для корректной перегрузки оператора += в MSVC.
static bool s_register_receive = []() -> bool {
    try {
        // Формируем std::function с точной сигнатурой rakhook::receive_t
        // rakhook::receive_t ожидается как: bool(Packet*& p)
        std::function<rakhook::receive_t> fn = [](Packet*& p) -> bool {
            return OnReceivePacket_Impl(p);
        };

        rakhook::on_receive_packet += fn;
    } catch (...) {
        OutputDebugStringA("[Plugin] failed to subscribe to rakhook::on_receive_packet\n");
        return false;
    }
    OutputDebugStringA("[Plugin] subscribed to rakhook::on_receive_packet\n");
    return true;
}();
// -------------------------------------------------------------------------------

/*
  Примечания и оговорки:

  1) Мы используем forward-declaration struct Packet; + proxy-struct для доступа к полям data/length.
     Это рабочий и распространённый приём в моддинге SA-MP, но он опирается на то, что
     layout структуры Packet в вашей версии RakNet совпадает с ожиданиями (data, length как первые поля).
     Если в вашей сборке layout иной — нужно включить реальный заголовок Packet.h или скорректировать proxy.

  2) Возвращаем true из обработчика — в RakHook это обычно означает "не блокировать оригинальную обработку".
     Если хотите блокировать пакет, возвращайте false.

  3) Если CI/компилятор снова ругается на несовпадение типа для std::function<rakhook::receive_t>,
     пришлите точное определение типа rakhook::receive_t (его можно найти в build_deps/rakhook-src/include/.../RakHook.hpp),
     и я адаптирую сигнатуру/обёртку точно под него.

  4) Если хотите безопаснее работать с Packet, можно в CMake добавить путь к RakNet headers
     (чтобы включать RakNet/Packet.h) — тогда можно использовать оригинальный тип Packet напрямую
     и не прибегать к proxy. Но в вашем CI этот хедер отсутствует, поэтому мы обошли это без изменения CMake.

  5) Если main.cpp у вас уже вызывает rakhook::initialize(), не переживайте — подписка через on_receive_packet
     в статическом инициализаторе просто добавит обработчик в список; если rakhook ещё не инициализирован — он
     всё равно зарегистрируется в глобальном объекте on_event и начнёт срабатывать после инициализации.

*/

