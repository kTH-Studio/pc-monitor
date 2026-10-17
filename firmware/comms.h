#pragma once
#include <Arduino.h>
#include "config.h"
#include "overlays.h"
#include "lang.h" 

//  Связь с ПК (приём данных + отправка команд)

//  Канал связи 
enum CommChannel : uint8_t {
    CH_NONE = 0,
    CH_SERIAL,
    CH_TCP,
    CH_HTTP
};

//  Инициализация 
void comms_init();
void comms_update();

//  Состояние 
bool        comms_is_linked();       // есть ли связь с ПК (данные идут)
CommChannel comms_channel();         // текущий активный канал
uint32_t    comms_last_rx_ms();      // millis последнего пакета

//  Отправка команд на ПК 
void comms_send_power(PowerAction act);   // сон/перезагрузка/выключение
void comms_send_media(const char* cmd);   // "next", "prev", "playpause"
void comms_send_lang(Lang l);             // синхронизация языка
void comms_send_reset(const char* what);   // "net" / "leak" / "fps" / "all" / "counters"

//  Ручной приём (для HTTP-хендлера) 
void comms_handle_http_body(const uint8_t* data, size_t len);
