#include "pages.h"
#include "display.h"
#include "input.h"
#include "settings.h"
#include "sensors.h"
#include "lang.h"
#include <math.h>
#include "symbols.h"

//  Глобальные данные с ПК

GpuData     g_gpu     = {};
CpuData     g_cpu     = {};
RamData     g_ram     = {};
DiskData    g_disks[4] = {};
uint8_t     g_diskCount = 0;
NetData     g_net     = {};
PingData    g_ping    = {};
LeakData    g_leak    = {};
ClimateData g_climate = {};

//  Тренды для больших значений (↑↓)
// Порог изменения — чтобы не дёргалось на шуме
#define TREND_THRESHOLD_LOAD  1     // для % (загрузка)
#define TREND_THRESHOLD_TEMP  1     // для °C
#define TREND_THRESHOLD_NET   0.5f  // для Mbit/s

enum Trend : int8_t {
    TREND_NONE = 0,
    TREND_UP   = 1,
    TREND_DOWN = -1,
};

// Предыдущие значения (обновляются при чтении)
struct PrevValues {
    // GPU
    float gpu_load, gpu_temp;
    // CPU
    float cpu_load, cpu_temp;
    // RAM
    float ram_load;
    // Disks (первый)
    float disk_load, disk_temp;
    // Net
    float net_down, net_up;
    // Climate
    float climate_temp, climate_hum;

    bool initialized;
};
static PrevValues _prev = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false };

// Общая функция: обновляет prev, возвращает тренд
static Trend calc_trend(float current, float& prev, float threshold) {
    if (!_prev.initialized) {
        prev = current;
        return TREND_NONE;
    }
    Trend t = TREND_NONE;
    float diff = current - prev;
    if (diff > threshold)       t = TREND_UP;
    else if (diff < -threshold) t = TREND_DOWN;
    prev = current;
    return t;
}

// Быстрые обёртки (обновляют _prev и возвращают тренд)
static Trend trend_gpu_load()    { return calc_trend(g_gpu.load,     _prev.gpu_load,    TREND_THRESHOLD_LOAD); }
static Trend trend_gpu_temp()    { return calc_trend(g_gpu.temp,     _prev.gpu_temp,    TREND_THRESHOLD_TEMP); }
static Trend trend_cpu_load()    { return calc_trend(g_cpu.load,     _prev.cpu_load,    TREND_THRESHOLD_LOAD); }
static Trend trend_cpu_temp()    { return calc_trend(g_cpu.temp,     _prev.cpu_temp,    TREND_THRESHOLD_TEMP); }
static Trend trend_ram_load()    { return calc_trend(g_ram.load,     _prev.ram_load,    TREND_THRESHOLD_LOAD); }
static Trend trend_disk_load()   { return calc_trend(g_disks[0].load, _prev.disk_load,  TREND_THRESHOLD_LOAD); }
static Trend trend_disk_temp()   { return calc_trend(g_disks[0].temp, _prev.disk_temp,  TREND_THRESHOLD_TEMP); }
static Trend trend_net_down()    { return calc_trend(g_net.downMbit, _prev.net_down,    TREND_THRESHOLD_NET); }
static Trend trend_net_up()      { return calc_trend(g_net.upMbit,   _prev.net_up,      TREND_THRESHOLD_NET); }
static Trend trend_climate_temp(){ return calc_trend(g_climate.temp, _prev.climate_temp,TREND_THRESHOLD_TEMP); }
static Trend trend_climate_hum() { return calc_trend(g_climate.hum,  _prev.climate_hum, TREND_THRESHOLD_TEMP); }

// Сброс при смене страницы (чтобы не было ложных стрелок)
void pages_reset_trends() {
    _prev.initialized = false;
}

//  Состояние ротации

static uint8_t  curPage = 0;         // id текущей страницы
static uint8_t  curOrder = 0;        // позиция в ротации (0..total-1)
static uint8_t  totalPages = 0;
static uint8_t  pageList[8];         // какие страницы сейчас активны
static uint32_t indicatorStart = 0;  // когда началась заливка текущей страницы

//  Список активных страниц

static void rebuild_page_list() {
    totalPages = 0;
    uint8_t order[8];
    uint8_t count = settings_get_page_count();
    for (uint8_t i = 0; i < count; i++) {
        order[i] = settings_get_page_order(i);
    }

    for (uint8_t i = 0; i < count; i++) {
        uint8_t id = order[i];

        // Условные страницы — проверяем наличие данных
        if (id == PG_LEAK && !g_leak.present) continue;
        if (id == PG_DISK && g_diskCount == 0) continue;

        pageList[totalPages++] = id;
        if (totalPages >= 8) break;
    }

    if (totalPages == 0) {
        pageList[totalPages++] = PG_GPU;
    }
    if (curOrder >= totalPages) curOrder = 0;
    curPage = pageList[curOrder];
}

void pages_init() {
    rebuild_page_list();
    curOrder = 0;
    curPage = pageList[0];
    indicatorStart = millis();
}

void pages_next() {
    pages_reset_trends();
    curOrder = (curOrder + 1) % totalPages;
    curPage = pageList[curOrder];
    indicatorStart = millis();
}

void pages_prev() {
    pages_reset_trends();
    curOrder = (curOrder == 0) ? totalPages - 1 : curOrder - 1;
    curPage = pageList[curOrder];
    indicatorStart = millis();
}

void pages_goto(uint8_t idx) {
    pages_reset_trends();
    if (idx >= totalPages) return;
    curOrder = idx;
    curPage  = pageList[curOrder];
    indicatorStart = millis();
}

uint8_t pages_current()       { return curPage; }
uint8_t pages_current_index() { return curOrder; }
uint8_t pages_total()         { return totalPages; }

//  Бегущая строка

// offset — смещение в пикселях (увеличивается со временем)
void draw_marquee(U8G2* u, int x, int y, int w, const char* text, uint32_t offset) {
    int textW = u->getStrWidth(text);
    if (textW <= w) {
        u->drawStr(x, y, text);
        return;
    }
    // Бегущая: рисуем два раза (зацикливание)
    int cycle = textW + 20;    // +20 — пауза между повторами
    int off = offset % cycle;
    u->drawStr(x - off, y, text);
    u->drawStr(x - off + cycle, y, text);
}

//  Индикатор страниц (внизу большого экрана)

void draw_page_indicator(U8G2* u, uint8_t current, uint8_t total, uint8_t progress_px) {
    if (total == 0) return;

    const int y = OLED64_H - PAGE_IND_H;  // нижняя строка
    const int gap = PAGE_IND_GAP;
    const int usable = OLED64_W - (total - 1) * gap;
    const int segW = usable / total;

    for (uint8_t i = 0; i < total; i++) {
        int x = i * (segW + gap);

        // Высота заливки этого сегмента
        uint8_t h = 0;
        if (i < current) {
            h = PAGE_IND_H;            // пройденные — залиты полностью
        } else if (i == current) {
            h = progress_px;           // текущий — растёт
            if (h > PAGE_IND_H) h = PAGE_IND_H;
        }

        if (h > 0) {
            u->drawBox(x, y + (PAGE_IND_H - h), segW, h);
        }
    }
}

//  Вспомогательные для отрисовки

static void fmt_gb(char* buf, size_t n, float v) {
    if (v >= 10)      snprintf(buf, n, "%.0f", v);
    else if (v >= 1)  snprintf(buf, n, "%.1f", v);
    else              snprintf(buf, n, "%.2f", v);
}

//  Рисование стрелки тренда
// Рисует стрелку слева от значения. Возвращает ширину стрелки + отступ.
// Если тренд NONE — ничего не рисует, возвращает 0.
static int draw_trend(U8G2* u, int x, int y_baseline, Trend t, int size = 7) {
    if (t == TREND_NONE) return 0;

    // y_baseline — базовая линия текста.
    // Стрелка рисуется над буквой, чуть выше.
    int arrowY = y_baseline - size - 1;

    uint8_t w;
    if (t == TREND_UP) {
        w = sym_arrow_up(u, x, arrowY, size);
    } else {
        w = sym_arrow_down(u, x, arrowY, size);
    }
    return w + 2;   // +2 отступ до числа
}

// Прогресс текущей страницы (для индикатора)
static uint8_t page_progress_px() {
    uint32_t elapsed = millis() - indicatorStart;
    uint32_t px = (elapsed / PAGE_IND_STEP_MS) * PAGE_IND_PX;
    if (px > PAGE_IND_H) px = PAGE_IND_H;
    return (uint8_t)px;
}

// Смещение бегущих строк (общее)
static uint32_t marqueeOffset() {
    return millis() / 40;    // ~25 px/сек
}

//  РЕНДЕР БОЛЬШОГО ЭКРАНА

static void render_gpu_big(U8G2* u) {
    u->setFont(FONT_BIG_NORMAL);

    if (!g_gpu.present) {
        u->drawStr(0, 10, "GPU: not found");
        return;
    }

    // Заголовок
    char title[40];
    snprintf(title, sizeof(title), "GPU  %s", g_gpu.name);
    draw_marquee(u, 0, 10, 128, title, marqueeOffset());

    // Тренды (считаем ОДИН раз за кадр, чтобы prev обновилось корректно)
    Trend tLoad = trend_gpu_load();
    Trend tTemp = trend_gpu_temp();

    // === Строка 3: крупные значения ===
    u->setFont(FONT_BIG_HUGE);
    char buf[16];

    // --- Load % слева ---
    int x = 0;
    snprintf(buf, sizeof(buf), "%d%%", g_gpu.load);
    int wLoad = u->getStrWidth(buf);

    // Стрелка (если есть) слева от числа
    int arrowW = 0;
    if (tLoad != TREND_NONE) {
        arrowW = draw_trend(u, x, 36, tLoad, 7);
    }
    u->drawStr(x + arrowW, 36, buf);

    // --- Temp °C справа ---
    snprintf(buf, sizeof(buf), "%d°C", g_gpu.temp);
    int wTemp = u->getStrWidth(buf);
    int xTemp = 128 - wTemp;
    if (tTemp != TREND_NONE) {
        xTemp -= 9;    // место под стрелку
        draw_trend(u, xTemp, 36, tTemp, 7);
        xTemp += 9;
    }
    u->drawStr(xTemp, 36, buf);

    // === Строка 5: VRAM и power (мелко) ===
    u->setFont(FONT_BIG_NORMAL);
    char gb1[8], gb2[8];
    fmt_gb(gb1, sizeof(gb1), g_gpu.vramUsed);
    fmt_gb(gb2, sizeof(gb2), g_gpu.vramTotal);
    snprintf(buf, sizeof(buf), "%s/%sGB", gb1, gb2);
    u->drawStr(0, 48, buf);

    snprintf(buf, sizeof(buf), "%dW", g_gpu.power);
    int w = u->getStrWidth(buf);
    u->drawStr(128 - w, 48, buf);

    // === Строка 6: fan + driver ===
    snprintf(buf, sizeof(buf), "%dRPM", g_gpu.fan);
    u->drawStr(0, 58, buf);

    snprintf(buf, sizeof(buf), "drv %s", g_gpu.driver);
    w = u->getStrWidth(buf);
    u->drawStr(128 - w, 58, buf);
}

static void render_cpu_big(U8G2* u) {
    if (!g_cpu.present) {
        u->setFont(FONT_BIG_NORMAL);
        u->drawStr(0, 10, "CPU: not found");
        return;
    }

    u->setFont(FONT_BIG_NORMAL);
    char title[40];
    snprintf(title, sizeof(title), "CPU  %s", g_cpu.name);
    draw_marquee(u, 0, 10, 128, title, marqueeOffset());

    Trend tLoad = trend_cpu_load();
    Trend tTemp = trend_cpu_temp();

    u->setFont(FONT_BIG_HUGE);
    char buf[16];

    // Load
    int x = 0;
    snprintf(buf, sizeof(buf), "%d%%", g_cpu.load);
    int arrowW = 0;
    if (tLoad != TREND_NONE) {
        arrowW = draw_trend(u, x, 36, tLoad, 7);
    }
    u->drawStr(x + arrowW, 36, buf);

    // Temp
    snprintf(buf, sizeof(buf), "%d°C", g_cpu.temp);
    int wTemp = u->getStrWidth(buf);
    int xTemp = 128 - wTemp;
    if (tTemp != TREND_NONE) {
        xTemp -= 9;
        draw_trend(u, xTemp, 36, tTemp, 7);
        xTemp += 9;
    }
    u->drawStr(xTemp, 36, buf);

    // Строка 5
    u->setFont(FONT_BIG_NORMAL);
    snprintf(buf, sizeof(buf), "%.2fGHz", g_cpu.freq / 1000.0f);
    u->drawStr(0, 48, buf);

    snprintf(buf, sizeof(buf), "%dW", g_cpu.power);
    int w = u->getStrWidth(buf);
    u->drawStr(58 - w / 2, 48, buf);

    snprintf(buf, sizeof(buf), "%d°C", g_cpu.coreMax);
    w = u->getStrWidth(buf);
    u->drawStr(128 - w, 48, buf);

    // Строка 6
    snprintf(buf, sizeof(buf), "%dRPM", g_cpu.fan);
    u->drawStr(0, 58, buf);

    snprintf(buf, sizeof(buf), "%.2fV", g_cpu.vcore / 1000.0f);
    w = u->getStrWidth(buf);
    u->drawStr(128 - w, 58, buf);
}

static void render_ram_big(U8G2* u) {
    if (!g_ram.present) {
        u->setFont(FONT_BIG_NORMAL);
        u->drawStr(0, 10, "RAM: not found");
        return;
    }
    u->setFont(FONT_BIG_NORMAL);
    char title[40];
    snprintf(title, sizeof(title), "RAM  %s", g_ram.name);
    draw_marquee(u, 0, 10, 128, title, marqueeOffset());

    Trend tLoad = trend_ram_load();

    u->setFont(FONT_BIG_HUGE);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", g_ram.load);

    int arrowW = 0;
    if (tLoad != TREND_NONE) {
        arrowW = draw_trend(u, 0, 36, tLoad, 7);
    }
    u->drawStr(arrowW, 36, buf);

    // used/total справа
    char u1[8], u2[8];
    fmt_gb(u1, sizeof(u1), g_ram.used);
    fmt_gb(u2, sizeof(u2), g_ram.total);
    snprintf(buf, sizeof(buf), "%s/%sGB", u1, u2);
    u->setFont(FONT_BIG_NORMAL);
    int w = u->getStrWidth(buf);
    u->drawStr(128 - w, 36, buf);

    // Строка 5
    u->setFont(FONT_BIG_NORMAL);
    snprintf(buf, sizeof(buf), "%dMHz", g_ram.freq);
    u->drawStr(0, 48, buf);
    u->drawStr(70, 48, g_ram.type);
    snprintf(buf, sizeof(buf), "%d°C", g_ram.temp);
    w = u->getStrWidth(buf);
    u->drawStr(128 - w, 48, buf);

    // Строка 6
    snprintf(buf, sizeof(buf), "%.2fV", g_ram.voltage / 1000.0f);
    u->drawStr(0, 58, buf);
    u->drawStr(60, 58, g_ram.dualChannel ? "Dual" : "Single");
}

static void render_disk_big(U8G2* u) {
    DiskData& d = g_disks[0];

    if (!d.present) {
        u->setFont(FONT_BIG_NORMAL);
        u->drawStr(0, 10, "DISK: not found");
        return;
    }

    u->setFont(FONT_BIG_NORMAL);
    char title[40];
    snprintf(title, sizeof(title), "%c:  %s", d.letter, d.name);
    draw_marquee(u, 0, 10, 128, title, marqueeOffset());

    Trend tLoad = trend_disk_load();
    Trend tTemp = trend_disk_temp();

    u->setFont(FONT_BIG_HUGE);
    char buf[16];

    // Load
    snprintf(buf, sizeof(buf), "%d%%", d.load);
    int arrowW = 0;
    if (tLoad != TREND_NONE) {
        arrowW = draw_trend(u, 0, 36, tLoad, 7);
    }
    u->drawStr(arrowW, 36, buf);

    // Temp
    snprintf(buf, sizeof(buf), "%d°C", d.temp);
    int wTemp = u->getStrWidth(buf);
    int xTemp = 128 - wTemp;
    if (tTemp != TREND_NONE) {
        xTemp -= 9;
        draw_trend(u, xTemp, 36, tTemp, 7);
        xTemp += 9;
    }
    u->drawStr(xTemp, 36, buf);

    // Строка 5
    u->setFont(FONT_BIG_NORMAL);
    char u1[8], u2[8];
    fmt_gb(u1, sizeof(u1), d.used);
    fmt_gb(u2, sizeof(u2), d.total);
    snprintf(buf, sizeof(buf), "%s/%sGB", u1, u2);
    u->drawStr(0, 48, buf);
    snprintf(buf, sizeof(buf), "%dMB/s", d.maxSpeed);
    int w = u->getStrWidth(buf);
    u->drawStr(74, 48, buf);
    u->drawStr(112, 48, d.type);

    // Строка 6
    snprintf(buf, sizeof(buf), "%d/%dMB/s", d.readRate, d.writeRate);
    u->drawStr(0, 58, buf);
    snprintf(buf, sizeof(buf), "%d%%", d.lifeLeft);
    w = u->getStrWidth(buf);
    u->drawStr(128 - w, 58, buf);
}

static void render_net_big(U8G2* u) {
    if (!g_net.present) {
        u->setFont(FONT_BIG_NORMAL);
        u->drawStr(0, 10, "NET: not found");
        return;
    }
    u->setFont(FONT_BIG_NORMAL);
    char title[40];
    snprintf(title, sizeof(title), "NET  %s", g_net.adapter);
    draw_marquee(u, 0, 10, 128, title, marqueeOffset());

    Trend tDown = trend_net_down();
    Trend tUp   = trend_net_up();

    // Крупно: скорости с трендами
    u->setFont(FONT_BIG_LARGE);
    char buf[24];

    // Download (слева)
    snprintf(buf, sizeof(buf), "v%.1f", g_net.downMbit);
    int arrowW = 0;
    if (tDown != TREND_NONE) {
        arrowW = draw_trend(u, 0, 36, tDown, 8);
    }
    u->drawStr(arrowW, 36, buf);

    // Upload (справа)
    snprintf(buf, sizeof(buf), "^%.1f", g_net.upMbit);
    int wUp = u->getStrWidth(buf);
    int xUp = 128 - wUp;
    if (tUp != TREND_NONE) {
        xUp -= 10;
        draw_trend(u, xUp, 36, tUp, 8);
        xUp += 10;
    }
    u->drawStr(xUp, 36, buf);

    // Единица измерения по центру
    u->setFont(FONT_BIG_SMALL);
    u->drawStr(58, 22, "Mbit/s");

    // Строка 5
    u->setFont(FONT_BIG_NORMAL);
    char gb1[8], gb2[8];
    fmt_gb(gb1, sizeof(gb1), g_net.totalDownGB);
    fmt_gb(gb2, sizeof(gb2), g_net.totalUpGB);
    snprintf(buf, sizeof(buf), "%sGB / %sGB", gb1, gb2);
    u->drawStr(0, 48, buf);

    // Строка 6
    u->drawStr(0, 58, g_net.ip);
}

static void render_ping_big(U8G2* u) {
    if (!g_ping.present) {
        u->setFont(FONT_BIG_NORMAL);
        u->drawStr(0, 10, "PING: --");
        return;
    }
    u->setFont(FONT_BIG_NORMAL);
    char buf[32];
    snprintf(buf, sizeof(buf), "PING  avg %dms", g_ping.avgMs);
    u->drawStr(0, 10, buf);

    u->setFont(FONT_BIG_SMALL);
    uint8_t y = 20;
    for (uint8_t i = 0; i < g_ping.count && i < 5; i++) {
        snprintf(buf, sizeof(buf), "%-12s %4dms", g_ping.targets[i].name, g_ping.targets[i].ms);
        u->drawStr(0, y, buf);
        y += 9;
    }
}

static void render_leak_big(U8G2* u) {
    u->setFont(FONT_BIG_NORMAL);
    u->drawStr(0, 10, "LEAK");

    u->setFont(FONT_BIG_SMALL);
    uint8_t y = 20;
    for (uint8_t i = 0; i < g_leak.count && i < 5; i++) {
        char buf[40];
        snprintf(buf, sizeof(buf), "%-14s +%luMB",
                 g_leak.items[i].name,
                 (unsigned long)g_leak.items[i].deltaMB);
        u->drawStr(0, y, buf);
        y += 9;
    }
}

static void render_climate_big(U8G2* u) {
    Trend tTemp = trend_climate_temp();
    Trend tHum  = trend_climate_hum();

    // Строка 1: температура/влажность крупно
    u->setFont(FONT_BIG_LARGE);
    char buf[16];

    // Температура
    snprintf(buf, sizeof(buf), "%.1f°C", g_climate.temp);
    int arrowW = 0;
    if (tTemp != TREND_NONE) {
        arrowW = draw_trend(u, 0, 18, tTemp, 8);
    }
    u->drawStr(arrowW, 18, buf);

    // Влажность
    snprintf(buf, sizeof(buf), "%.0f%%", g_climate.hum);
    int wHum = u->getStrWidth(buf);
    int xHum = 128 - wHum;
    if (tHum != TREND_NONE) {
        xHum -= 10;
        draw_trend(u, xHum, 18, tHum, 8);
        xHum += 10;
    }
    u->drawStr(xHum, 18, buf);

    // Строка 2: давление/свет мелко
    u->setFont(FONT_BIG_NORMAL);
    snprintf(buf, sizeof(buf), "%.0fmm", g_climate.pressMm);
    u->drawStr(0, 30, buf);
    snprintf(buf, sizeof(buf), "%d%%", g_climate.light);
    int w = u->getStrWidth(buf);
    u->drawStr(128 - w, 30, buf);

    // Строки 4-5: время + дата
    extern bool clock_get(int& h, int& m, int& d, int& mon, int& y, int& wd);
    int h, m, d, mon, y, wd;
    if (clock_get(h, m, d, mon, y, wd)) {
        u->setFont(FONT_BIG_HUGE);
        snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
        u->drawStr(0, 58, buf);

        u->setFont(FONT_BIG_SMALL);
        snprintf(buf, sizeof(buf), "%d %s %d", d, T_Month(mon - 1), y);
        w = u->getStrWidth(buf);
        u->drawStr(128 - w, 46, buf);

        u->drawStr(128 - u->getStrWidth(T_Day(wd - 1)), 58, T_Day(wd - 1));
    }
}

//  РЕНДЕР МАЛОГО ЭКРАНА (следующая страница)

static void render_gpu_small(U8G2* u) {
    u->setFont(FONT_SMALL);
    if (!g_gpu.present) { u->drawStr(0, 7, "GPU --"); return; }

    char buf[40];
    snprintf(buf, sizeof(buf), "GPU %s", g_gpu.name);
    u->drawStr(0, 7, buf);
    snprintf(buf, sizeof(buf), "%d%%  %d°C", g_gpu.load, g_gpu.temp);
    u->drawStr(0, 15, buf);
    char gb1[8], gb2[8];
    fmt_gb(gb1, sizeof(gb1), g_gpu.vramUsed);
    fmt_gb(gb2, sizeof(gb2), g_gpu.vramTotal);
    snprintf(buf, sizeof(buf), "%s/%sGB %dW", gb1, gb2, g_gpu.power);
    u->drawStr(0, 23, buf);
    snprintf(buf, sizeof(buf), "%dRPM drv%s", g_gpu.fan, g_gpu.driver);
    u->drawStr(0, 31, buf);
}

static void render_cpu_small(U8G2* u) {
    u->setFont(FONT_SMALL);
    if (!g_cpu.present) { u->drawStr(0, 7, "CPU --"); return; }
    char buf[40];
    snprintf(buf, sizeof(buf), "CPU %s", g_cpu.name);
    u->drawStr(0, 7, buf);
    snprintf(buf, sizeof(buf), "%d%%  %d°C", g_cpu.load, g_cpu.temp);
    u->drawStr(0, 15, buf);
    snprintf(buf, sizeof(buf), "%.2fGHz %dW %d°C",
             g_cpu.freq / 1000.0f, g_cpu.power, g_cpu.coreMax);
    u->drawStr(0, 23, buf);
    snprintf(buf, sizeof(buf), "%dRPM %.2fV", g_cpu.fan, g_cpu.vcore / 1000.0f);
    u->drawStr(0, 31, buf);
}

static void render_ram_small(U8G2* u) {
    u->setFont(FONT_SMALL);
    if (!g_ram.present) { u->drawStr(0, 7, "RAM --"); return; }
    char buf[40];
    snprintf(buf, sizeof(buf), "RAM %s", g_ram.name);
    u->drawStr(0, 7, buf);
    char u1[8], u2[8];
    fmt_gb(u1, sizeof(u1), g_ram.used);
    fmt_gb(u2, sizeof(u2), g_ram.total);
    snprintf(buf, sizeof(buf), "%d%%  %s/%sGB", g_ram.load, u1, u2);
    u->drawStr(0, 15, buf);
    snprintf(buf, sizeof(buf), "%dMHz %s %d°C", g_ram.freq, g_ram.type, g_ram.temp);
    u->drawStr(0, 23, buf);
    snprintf(buf, sizeof(buf), "%.2fV %s", g_ram.voltage / 1000.0f,
             g_ram.dualChannel ? "Dual" : "Single");
    u->drawStr(0, 31, buf);
}

static void render_disk_small(U8G2* u) {
    u->setFont(FONT_SMALL);
    DiskData& d = g_disks[0];
    if (!d.present) { u->drawStr(0, 7, "DISK --"); return; }
    char buf[40];
    snprintf(buf, sizeof(buf), "%c: %s", d.letter, d.name);
    u->drawStr(0, 7, buf);
    snprintf(buf, sizeof(buf), "%d%%  %d°C  %s", d.load, d.temp, d.type);
    u->drawStr(0, 15, buf);
    char u1[8], u2[8];
    fmt_gb(u1, sizeof(u1), d.used);
    fmt_gb(u2, sizeof(u2), d.total);
    snprintf(buf, sizeof(buf), "%s/%sGB %dMB/s", u1, u2, d.maxSpeed);
    u->drawStr(0, 23, buf);
    snprintf(buf, sizeof(buf), "%d/%dMB/s %d%%", d.readRate, d.writeRate, d.lifeLeft);
    u->drawStr(0, 31, buf);
}

static void render_net_small(U8G2* u) {
    u->setFont(FONT_SMALL);
    if (!g_net.present) { u->drawStr(0, 7, "NET --"); return; }
    char buf[40];
    snprintf(buf, sizeof(buf), "NET %s", g_net.adapter);
    u->drawStr(0, 7, buf);
    snprintf(buf, sizeof(buf), "v%.1f ^%.1f Mbit/s", g_net.downMbit, g_net.upMbit);
    u->drawStr(0, 15, buf);
    char gb1[8], gb2[8];
    fmt_gb(gb1, sizeof(gb1), g_net.totalDownGB);
    fmt_gb(gb2, sizeof(gb2), g_net.totalUpGB);
    snprintf(buf, sizeof(buf), "%sGB / %sGB", gb1, gb2);
    u->drawStr(0, 23, buf);
    u->drawStr(0, 31, g_net.ip);
}

static void render_ping_small(U8G2* u) {
    u->setFont(FONT_SMALL);
    if (!g_ping.present) { u->drawStr(0, 7, "PING --"); return; }
    char buf[40];
    snprintf(buf, sizeof(buf), "PING avg %dms", g_ping.avgMs);
    u->drawStr(0, 7, buf);
    // Топ-3 цели
    for (uint8_t i = 0; i < 3 && i < g_ping.count; i++) {
        snprintf(buf, sizeof(buf), "%s %dms",
                 g_ping.targets[i].name, g_ping.targets[i].ms);
        u->drawStr(0, 15 + i * 8, buf);
    }
}

static void render_leak_small(U8G2* u) {
    u->setFont(FONT_SMALL);
    u->drawStr(0, 7, "LEAK");
    for (uint8_t i = 0; i < 3 && i < g_leak.count; i++) {
        char buf[40];
        snprintf(buf, sizeof(buf), "%s +%luMB",
                 g_leak.items[i].name,
                 (unsigned long)g_leak.items[i].deltaMB);
        u->drawStr(0, 15 + i * 8, buf);
    }
}

static void render_climate_small(U8G2* u) {
    u->setFont(FONT_SMALL);
    char buf[40];

    snprintf(buf, sizeof(buf), "%.1f°C  %.0f%%", g_climate.temp, g_climate.hum);
    u->drawStr(0, 7, buf);
    snprintf(buf, sizeof(buf), "%.0fmm  %d%%", g_climate.pressMm, g_climate.light);
    u->drawStr(0, 15, buf);

    extern bool clock_get(int& h, int& m, int& d, int& mon, int& y, int& wd);
    int h, m, d, mon, y, wd;
    if (clock_get(h, m, d, mon, y, wd)) {
        snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
        u->drawStr(0, 23, buf);

        snprintf(buf, sizeof(buf), "%d %s %d", d, T_Month(mon - 1), y);
        u->drawStr(30, 23, buf);

        u->drawStr(0, 31, T_Day(wd - 1));
    }
}

//  Таблица рендереров

typedef void (*RenderFn)(U8G2*);

static const RenderFn RENDER_BIG[PG_COUNT] = {
    render_gpu_big, render_cpu_big, render_ram_big, render_disk_big,
    render_net_big, render_ping_big, render_leak_big, render_climate_big
};

static const RenderFn RENDER_SMALL[PG_COUNT] = {
    render_gpu_small, render_cpu_small, render_ram_small, render_disk_small,
    render_net_small, render_ping_small, render_leak_small, render_climate_small
};

//  Главный рендер

void pages_render() {
    // Проверяем, не изменился ли список (условные появились/исчезли)
    rebuild_page_list();

    U8G2* big   = display_big();
    U8G2* small = display_small();

    // БОЛЬШОЙ
    big->clearBuffer();
    if (curPage < PG_COUNT) {
        RENDER_BIG[curPage](big);
    }
    draw_page_indicator(big, curOrder, totalPages, page_progress_px());
    big->sendBuffer();

    // МАЛЫЙ (следующая страница)
    uint8_t nextOrder = (curOrder + 1) % totalPages;
    uint8_t nextPage = pageList[nextOrder];

    small->clearBuffer();
    if (nextPage < PG_COUNT) {
        RENDER_SMALL[nextPage](small);
    }
    // Индикатор малого — 1px внизу, поверх
    {
        const int y = OLED32_H - 1;
        const int gap = PAGE_IND_GAP;
        const int usable = OLED32_W - (totalPages - 1) * gap;
        const int segW = usable / totalPages;
        for (uint8_t i = 0; i < totalPages; i++) {
            int x = i * (segW + gap);
            uint8_t h = 0;
            if (i < nextOrder)      h = 1;
            else if (i == nextOrder) h = 1;
            if (h > 0) small->drawBox(x, y, segW, h);
        }
    }
    small->sendBuffer();

    // LED
    pages_update_leds();
}

//  LED по текущей странице

void pages_update_leds() {
    switch (curPage) {
        case PG_GPU: {
            led_update_accumulative(g_gpu.load);
            break;
        }
        case PG_CPU: {
            led_update_accumulative(g_cpu.load);
            break;
        }
        case PG_RAM: {
            led_update_accumulative(g_ram.load);
            break;
        }
        case PG_DISK: {
            led_update_accumulative(g_disks[0].activity);
            break;
        }
        case PG_NET: {
            led_update_accumulative(g_net.channelLoad);
            break;
        }
        case PG_PING: {
            // Один LED по среднему пингу
            uint16_t p = g_ping.avgMs;
            uint8_t idx = LED_WHITE;
            if      (p <= 20)  idx = LED_WHITE;
            else if (p <= 50)  idx = LED_BLUE;
            else if (p <= 100) idx = LED_GREEN;
            else if (p <= 200) idx = LED_YELLOW;
            else               idx = LED_RED;
            led_update_single(idx);
            break;
        }
        case PG_LEAK: {
            // Один LED по максимальному приросту
            uint32_t m = g_leak.maxDeltaMB;
            uint8_t idx = LED_WHITE;
            if      (m < 100)  idx = LED_WHITE;
            else if (m < 150)  idx = LED_WHITE;
            else if (m < 250)  idx = LED_BLUE;
            else if (m < 400)  idx = LED_GREEN;
            else if (m < 600)  idx = LED_YELLOW;
            else               idx = LED_RED;
            led_update_single(idx);
            break;
        }
        case PG_CLIMATE: {
            // Один LED по температуре
            float t = g_climate.temp;
            uint8_t idx = LED_GREEN;
            if      (t < 18) idx = LED_BLUE;
            else if (t < 22) idx = LED_WHITE;
            else if (t < 26) idx = LED_GREEN;
            else if (t < 30) idx = LED_YELLOW;
            else             idx = LED_RED;
            led_update_single(idx);
            break;
        }
        default: break;
    }
}
