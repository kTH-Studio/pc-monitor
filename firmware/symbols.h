#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

//  Кастомные символы
//  by KTH-Studio
//  ТОКЕНЫ (управляющие ASCII 0x01–0x1F)
//  Вставляются в строку как \x01, \x02, ...
#define SYM_PLAY          "\x01"
#define SYM_PAUSE         "\x02"
#define SYM_STOP          "\x03"
#define SYM_NEXT          "\x04"
#define SYM_PREV          "\x05"
#define SYM_HEART         "\x06"
#define SYM_RUBLE         "\x07"   // ₽
#define SYM_NUMERO        "\x08"   // №
#define SYM_CHECK         "\x09"   // ✓
#define SYM_CROSS         "\x0A"   // ✗
#define SYM_ELLIPSIS      "\x0B"   // …
#define SYM_DOT           "\x0C"   // ●
#define SYM_ARROW_UP      "\x0D"   // ↑
#define SYM_ARROW_DOWN    "\x0E"   // ↓
#define SYM_ARROW_LEFT    "\x0F"   // ←
#define SYM_ARROW_RIGHT   "\x10"   // →
#define SYM_PERMILLE      "\x11"   // ‰
#define SYM_CAREOF        "\x12"   // ℅
#define SYM_EMPTY         "\x13"   // ∅
#define SYM_DELTA         "\x14"   // ∆
#define SYM_SQRT          "\x15"   // √
#define SYM_OMEGA         "\x16"   // Ω
#define SYM_PI_CAP        "\x17"   // Π
#define SYM_MU            "\x18"   // μ
#define SYM_PI            "\x19"   // π
#define SYM_NEQ           "\x1A"   // ≠
#define SYM_APPROX        "\x1B"   // ≈
#define SYM_INFINITY      "\x1C"   // ∞
#define SYM_PRIME         "\x1D"   // ′
#define SYM_DPRIME        "\x1E"   // ″
#define SYM_CENT          "\x1F"   // ¢

//  Дроби и верхние индексы — через модификатор '~'
//  В строке пишется: ~1 ~2 ~3 ~0 ~n ~h ~q ~t ...
// Пример: "1~h" → "1½"  (унарная, но работает как модификатор)
//  Функции рисования (примитивы)
//  x, y — координаты ЛЕВОГО ВЕРХНЕГО угла
//  size — высота символа в пикселях (по умолчанию 8)
//  Возврат: ширина символа

uint8_t sym_play  (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_pause (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_stop  (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_next  (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_prev  (U8G2* u, int x, int y, uint8_t size = 8);

uint8_t sym_arrow_up    (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_arrow_down  (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_arrow_left  (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_arrow_right (U8G2* u, int x, int y, uint8_t size = 8);

uint8_t sym_heart    (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_ruble    (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_numero   (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_check    (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_cross    (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_ellipsis (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_dot      (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_permille (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_careof   (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_empty    (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_delta    (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_sqrt     (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_omega    (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_pi_cap   (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_mu       (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_pi       (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_neq      (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_approx   (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_infinity (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_prime    (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_dprime   (U8G2* u, int x, int y, uint8_t size = 8);
uint8_t sym_cent     (U8G2* u, int x, int y, uint8_t size = 8);

// Дроби (num/den — односимвольные "1".."9")
uint8_t sym_fraction (U8G2* u, int x, int y, uint8_t size,
                      char num, char den);

// Верхние индексы
uint8_t sym_superscript(U8G2* u, int x, int y, uint8_t size, char ch);

//  Главная функция: рисование строки с заменой токенов
//  - Обычный текст (UTF-8, включая кириллицу) — рисуется как есть.
//  - Управляющие токены 0x01–0x1F — заменяются на символы.
//  - Модификатор '~X' — верхний индекс или дробь.
//  Возврат: ширина нарисованной строки.
int draw_string_with_symbols(U8G2* u, int x, int y, const char* str);

//  Регион-коды (RU, EN, US, DE, ...)
//  Двухбуквенный код без рамок и иконок.
//  Возвращает ширину.
uint8_t draw_region_code(U8G2* u, int x, int y, const char* code);

// Предустановленные коды
#define REGION_RU   "RU"
#define REGION_EN   "EN"
#define REGION_US   "US"
#define REGION_DE   "DE"
#define REGION_CN   "CN"
#define REGION_JP   "JP"