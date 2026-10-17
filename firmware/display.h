#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

//  Дисплеи

// Инициализация обоих OLED
void display_init();

// Большой OLED (128×64)
U8G2* display_big();       // доступ к объекту
void  display_big_clear();
void  display_big_send();

// Малый OLED (128×32)
U8G2* display_small();
void  display_small_clear();
void  display_small_send();

// Яркость
void display_set_brightness(uint8_t pct);         // 0..100
void display_set_night_brightness(uint8_t pct);   // 0..100

// Boot-экраны
void display_boot_init_begin();
void display_boot_init_update();
bool display_boot_done();
void display_logo_show();

//  Шрифты (кириллица)

// Большой экран
#define FONT_BIG_TITLE   u8g2_font_7x13_t_cyrillic
#define FONT_BIG_NORMAL  u8g2_font_6x12_t_cyrillic
#define FONT_BIG_SMALL   u8g2_font_5x7_t_cyrillic
#define FONT_BIG_LARGE   u8g2_font_10x20_t_cyrillic
#define FONT_BIG_HUGE    u8g2_font_helvB18_t_cyrillic

// Малый экран
#define FONT_SMALL       u8g2_font_5x7_t_cyrillic
#define FONT_SMALL_BOLD  u8g2_font_6x12_t_cyrillic
