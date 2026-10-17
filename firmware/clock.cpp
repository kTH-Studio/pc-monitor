#include "clock.h"
#include <time.h>
#include <WiFi.h>

//  Состояние

static bool     synced    = false;
static int8_t   tzOffset  = 3;     // Москва UTC+3
static bool     ntpStarted = false;

//  Инициализация

void clock_init() {
    // Пока ничего — время поставим либо с ПК, либо через NTP
    Serial.println("[CLOCK] initialized");
}

//  Установка с ПК

void clock_set_from_pc(uint32_t unixTime, int8_t tzOffsetHours) {
    if (unixTime == 0) return;

    tzOffset = tzOffsetHours;

    // Устанавливаем системное время ESP (UTC)
    struct timeval tv;
    tv.tv_sec  = unixTime;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);

    synced = true;
    Serial.printf("[CLOCK] set from PC: %u (tz=%d)\n", unixTime, tzOffsetHours);
}

//  NTP fallback (если ПК не дал время)

static void start_ntp() {
    if (ntpStarted) return;
    ntpStarted = true;

    configTime(tzOffset * 3600, 0, "pool.ntp.org", "time.google.com");
    Serial.println("[CLOCK] NTP started");
}

//  Апдейт — если через 10 сек после WiFi время не пришло — NTP

void clock_update() {
    if (synced) return;

    // Если WiFi подключён — запускаем NTP как fallback
    if (WiFi.status() == WL_CONNECTED) {
        static uint32_t lastTry = 0;
        if (millis() - lastTry > 10000) {
            lastTry = millis();
            if (!ntpStarted) start_ntp();

            time_t now = time(nullptr);
            if (now > 1600000000) {   // > сентябрь 2020
                synced = true;
                Serial.printf("[CLOCK] NTP OK: %ld\n", (long)now);
            }
        }
    }
}

//  Получить текущее время

bool clock_get(int& h, int& m, int& d, int& mon, int& y, int& wd) {
    if (!synced) {
        h = m = 0;
        d = mon = 1;
        y = 2024;
        wd = 1;
        return false;
    }

    time_t now = time(nullptr);
    // Применяем смещение
    now += tzOffset * 3600;

    struct tm* t = gmtime(&now);

    h   = t->tm_hour;
    m   = t->tm_min;
    d   = t->tm_mday;
    mon = t->tm_mon + 1;
    y   = t->tm_year + 1900;
    wd  = t->tm_wday == 0 ? 7 : t->tm_wday;   // 1=Пн ... 7=Вс

    return true;
}

bool clock_synced() { return synced; }
