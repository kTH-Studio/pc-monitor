#pragma once
#include <Arduino.h>
#include "config.h"

//  Часы (синхронизация с ПК + NTP fallback)

void clock_init();
void clock_update();

// Установка времени с ПК (Unix timestamp)
void clock_set_from_pc(uint32_t unixTime, int8_t tzOffsetHours);

// Получить текущее время
// h: 0..23, m: 0..59, d: 1..31, mon: 1..12, y: 2024.., wd: 1..7 (1=Пн)
bool clock_get(int& h, int& m, int& d, int& mon, int& y, int& wd);

// Синхронизированы ли часы хоть раз
bool clock_synced();
