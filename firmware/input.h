#pragma once
#include <Arduino.h>
#include "config.h"
#include "settings.h"

//  Кнопки + LED

// Тайминги кнопок
#define BTN_DEBOUNCE_MS   30
#define BTN_LONG_MS       800     // удержание = долгое
#define BTN_DOUBLE_MS     300     // окно для двойного

//  Событие нажатия (что случилось) 
struct ButtonEvent {
    uint8_t     btn;      // 0..4
    PressType   type;     // короткое / долгое / двойное
    bool        valid;    // есть ли событие
};

//  Инициализация 
void input_init();
void input_update();

//  Получить событие (или пустое) 
ButtonEvent input_get_event();

//  LED 
void led_all_off();
void led_set(uint8_t idx, bool on);           // idx: 0=белый..4=красный
void led_set_raw(uint8_t pin, bool on);

// Режимы по страницам
void led_update_accumulative(uint8_t percent); // 5 ступеней по 20% (для GPU/CPU/ОЗУ/Диск/Сеть)
void led_update_single(uint8_t idx);           // горит только один (для Пинг/Утечка/Климат)
void led_flash_white(uint16_t duration_ms);    // вспышка белым (нотификация)
void led_blink_red(uint16_t duration_ms);      // мигание красным (питание)
void led_tick();                               // обновление анимаций (flash, blink)

//  Порядок LED (физический)

// idx 0 = белый, 1 = синий, 2 = зелёный, 3 = жёлтый, 4 = красный
#define LED_WHITE   0
#define LED_BLUE    1
#define LED_GREEN   2
#define LED_YELLOW  3
#define LED_RED     4
