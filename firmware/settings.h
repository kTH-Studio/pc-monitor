#pragma once
#include <Arduino.h>
#include "config.h"
#include "lang.h"

//  Настройки в NVS

// Действия кнопок
enum ButtonAction : uint8_t {
    ACT_NONE = 0,
    ACT_NEXT_PAGE,
    ACT_PREV_PAGE,
    ACT_NEXT_TRACK,
    ACT_PREV_TRACK,
    ACT_PLAY_PAUSE,
    ACT_NIGHT_TOGGLE,
    ACT_LANG_TOGGLE,
    ACT_POWER_SLEEP,
    ACT_POWER_REBOOT,
    ACT_POWER_OFF,
    ACT_POWER_CANCEL,
    ACT_ESP_REBOOT,
    ACT_RESET_COUNTERS,
    ACT_COUNT
};

// Типы нажатий
enum PressType : uint8_t {
    PRESS_SHORT = 0,
    PRESS_LONG,
    PRESS_DOUBLE,
    PRESS_COUNT
};

// Режим ночного
enum NightMode : uint8_t {
    NIGHT_OFF = 0,
    NIGHT_MANUAL,
    NIGHT_BY_TIME
};

// Пинг-цель
struct PingTarget {
    char name[24];
    char host[40];
};

// Пороги для LED и уведомлений
struct Thresholds {
    uint8_t gpuTemp;
    uint8_t cpuTemp;
    uint8_t ramPct;
    uint8_t diskPct;
    uint16_t pingMs;
    uint16_t leakMB;
};

//  API

void settings_init();          // загрузить всё из NVS (или дефолты)
void settings_save();          // сохранить всё в NVS
void settings_reset();         // сброс к дефолтам

// Язык
Lang  settings_get_lang();
void  settings_set_lang(Lang l);

// WiFi
bool     settings_has_wifi();
String   settings_get_wifi_ssid();
String   settings_get_wifi_pass();
void     settings_set_wifi(const String& ssid, const String& pass);
void     settings_clear_wifi();

// Отображение
uint8_t settings_get_brightness();
void    settings_set_brightness(uint8_t val);
uint8_t settings_get_night_brightness();
void    settings_set_night_brightness(uint8_t val);

// Ночной режим
NightMode settings_get_night_mode();
void      settings_set_night_mode(NightMode m);
uint8_t   settings_get_night_hour_on();
uint8_t   settings_get_night_hour_off();
void      settings_set_night_time(uint8_t on_h, uint8_t off_h);

// Тайминги
uint16_t settings_get_notify_duration();
void     settings_set_notify_duration(uint16_t ms);
uint16_t settings_get_media_pause();
void     settings_set_media_pause(uint16_t ms);
uint16_t settings_get_power_confirm();
void     settings_set_power_confirm(uint16_t ms);

// Пороги
Thresholds settings_get_thresholds();
void       settings_set_thresholds(const Thresholds& t);

// Пинг-цели
uint8_t       settings_get_ping_count();
PingTarget    settings_get_ping(uint8_t idx);
void          settings_set_ping(uint8_t idx, const char* name, const char* host);

// Порядок страниц
// page id: 0=GPU,1=CPU,2=RAM,3=DISK,4=NET,5=PING,6=LEAK,7=CLIMATE
uint8_t  settings_get_page_order(uint8_t idx);
void     settings_set_page_order(const uint8_t* order, uint8_t count);
uint8_t  settings_get_page_count();

// Кнопки
ButtonAction settings_get_button(uint8_t btn, PressType type);
void         settings_set_button(uint8_t btn, PressType type, ButtonAction act);

// Медиа-приоритет список строк через ';'
String settings_get_media_priority();
void   settings_set_media_priority(const String& list);

// Условия страниц
// idx: 0=leak, 1=disks, 2=fps
// val: "auto" / "always" / "never"
const char* settings_get_condition(uint8_t idx);
void        settings_set_condition(uint8_t idx, const char* val);