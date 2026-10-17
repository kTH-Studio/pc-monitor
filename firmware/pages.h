#pragma once
#include <Arduino.h>
#include "config.h"
#include <U8G2lib.h> 

//  ID страниц (порядок в NVS может быть другим!)

enum PageId : uint8_t {
    PG_GPU      = 0,
    PG_CPU      = 1,
    PG_RAM      = 2,
    PG_DISK     = 3,
    PG_NET      = 4,
    PG_PING     = 5,
    PG_LEAK     = 6,
    PG_CLIMATE  = 7,
    PG_COUNT    = 8
};

//  Данные с ПК (заполняет comms.cpp)

struct GpuData {
    bool     present;
    char     name[32];
    uint8_t  load;         // %
    uint8_t  temp;         // °C
    float    vramUsed;     // GB
    float    vramTotal;    // GB
    uint16_t power;        // W
    uint16_t fan;          // RPM
    char     driver[16];
};

struct CpuData {
    bool     present;
    char     name[32];
    uint8_t  load;         // %
    uint8_t  temp;         // °C
    uint16_t freq;         // MHz
    uint16_t power;        // W
    uint8_t  coreMax;      // °C
    uint16_t fan;          // RPM
    uint16_t vcore;        // mV (1.25 V = 1250)
};

struct RamData {
    bool     present;
    char     name[32];
    uint8_t  load;         // %
    float    used;         // GB
    float    total;        // GB
    uint16_t freq;         // MHz
    char     type[8];      // DDR4/DDR5
    uint8_t  temp;         // °C
    uint16_t voltage;      // mV
    bool     dualChannel;
};

struct DiskData {
    bool     present;
    char     letter;       // 'C'
    char     name[32];
    uint8_t  load;         // % занято
    uint8_t  temp;         // °C
    float    used;         // GB
    float    total;        // GB
    uint16_t maxSpeed;     // MB/s
    char     type[8];      // NVMe/SATA/HDD
    uint16_t readRate;     // MB/s
    uint16_t writeRate;    // MB/s
    uint8_t  lifeLeft;     // % (remaining life)
    uint8_t  activity;     // % (для LED)
};

struct NetData {
    bool     present;
    char     adapter[32];
    float    downMbit;
    float    upMbit;
    float    totalDownGB;
    float    totalUpGB;
    char     ip[16];
    uint16_t linkSpeed;    // Mbit/s
    uint8_t  channelLoad;  // % (для LED)
};

// Результат пинга (name + ms). НЕ путать с PingTarget из settings.h (name + host).
struct PingResult {
    char     name[24];
    uint16_t ms;
};

struct PingData {
    bool     present;
    uint8_t  count;
    PingResult targets[5];    // было PingTarget
    uint16_t avgMs;
};

struct LeakData {
    bool     present;      // true, если есть утечка >100MB
    uint8_t  count;
    struct {
        char     name[24];
        uint32_t deltaMB;
    } items[5];
    uint32_t maxDeltaMB;
};

struct ClimateData {
    bool     present;
    float    temp;
    float    hum;
    float    pressMm;
    uint8_t  light;
};

// Глобальные данные
extern GpuData     g_gpu;
extern CpuData     g_cpu;
extern RamData     g_ram;
extern DiskData    g_disks[4];
extern uint8_t     g_diskCount;
extern NetData     g_net;
extern PingData    g_ping;
extern LeakData    g_leak;
extern ClimateData g_climate;


//  API страниц

void pages_init();
void pages_next();
void pages_prev();
void pages_render();
void pages_goto(uint8_t idx);
uint8_t pages_current();        // id текущей
uint8_t pages_current_index();  // порядковый номер в ротации
uint8_t pages_total();          // сколько страниц сейчас (с учётом условных)

//  Хелперы отрисовки (для использования в overlays.cpp)

// Бегущая строка: text, позиция x/y, ширина окна, скорость
void draw_marquee(U8G2* u, int x, int y, int w, const char* text, uint32_t offset);

// Индикатор страниц (большой экран, внизу)
void draw_page_indicator(U8G2* u, uint8_t current, uint8_t total, uint8_t progress_px);

// LED для текущей страницы
void pages_update_leds();

// Тренды
void pages_reset_trends();