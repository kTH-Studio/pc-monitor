#pragma once
#include <Arduino.h>
#include "config.h"

//  Оверлеи поверх страниц

// Тип оверлея (по приоритету)
enum OverlayType : uint8_t {
    OV_NONE = 0,
    OV_MEDIA,           // малый: плеер
    OV_POWER_CONFIRM,   // малый: подтверждение выключения
    OV_NOTIFY,          // большой: уведомление
    OV_FPS,             // большой: FPS (главный)
};

// Медиа-данные (заполняет comms)
struct MediaData {
    bool     present;      // есть активный плеер
    bool     playing;      // играет или пауза
    char     app[24];      // Spotify / YouTube / ...
    char     artist[40];
    char     track[64];
    char     album[64];
    uint16_t posSec;
    uint16_t durSec;
    bool     updated;      // флаг, что данные изменились (для сброса паузы)
};

// Нотификация
struct NotifyData {
    char     app[24];
    uint8_t  count;        // +N
    char     title[48];
    char     body[200];    // может быть длинным — обрежется на экране
    char     time[16];     // "14:32" или "8.10 14:32"
};

// FPS
struct FpsData {
    bool     present;
    char     game[48];
    uint16_t current;
    uint16_t minFps;
    uint16_t maxFps;
    uint8_t  dropPct;
    uint8_t  gpuLoad;
    uint8_t  cpuLoad;
};

// Подтверждение питания
enum PowerAction : uint8_t {
    PWR_NONE = 0,
    PWR_SLEEP,
    PWR_REBOOT,
    PWR_OFF
};

//  Глобальные данные оверлеев

extern MediaData  g_media;
extern FpsData    g_fps;

//  API

void overlays_init();
bool overlays_update();       // true — оверлей активен (страницы не рисуются)

// Медиа
void media_set(const MediaData& m);  // вызывается из comms

// Нотификации
void notify_push(const NotifyData& n); // в очередь

// FPS
void fps_set(const FpsData& f);

// Питание
void power_request(PowerAction act);   // запустить отсчёт
void power_cancel();                   // отмена
bool power_active();                   // идёт ли отсчёт
PowerAction power_current();           // какое действие
