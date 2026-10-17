#pragma once
#include <Arduino.h>
#include "config.h"

//  Датчики климата (BME280 + LDR)

//  Инициализация 
void sensors_init();

//  Опрос (раз в SENSOR_POLL_MS) 
void sensors_update();
void sensors_start_polling();

//  BME280 
bool  sensors_bme_ok();
float sensors_temp();          // °C
float sensors_hum();           // %
float sensors_press_hpa();     // гПа
float sensors_press_mm();      // мм рт. ст.

//  LDR 
uint8_t sensors_light_pct();   // 0..100 %
bool    sensors_ldr_ok();
