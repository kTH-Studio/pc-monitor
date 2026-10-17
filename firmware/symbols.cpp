#include "symbols.h"

//  МЕДИА

uint8_t sym_play(U8G2* u, int x, int y, uint8_t size) {
    int w = (size * 3) / 4;
    for (int i = 0; i < w; i++) {
        int hh = (size * i) / w;
        int y1 = y + (size - hh) / 2;
        u->drawVLine(x + i, y1, hh);
    }
    return w;
}

uint8_t sym_pause(U8G2* u, int x, int y, uint8_t size) {
    int w = (size * 3) / 4;
    int bar = w / 3;
    if (bar < 1) bar = 1;
    u->drawBox(x, y, bar, size);
    u->drawBox(x + w - bar, y, bar, size);
    return w;
}

uint8_t sym_stop(U8G2* u, int x, int y, uint8_t size) {
    int w = (size * 3) / 4;
    u->drawBox(x, y, w, size);
    return w;
}

uint8_t sym_next(U8G2* u, int x, int y, uint8_t size) {
    int half = size / 2;
    for (int i = 0; i < half; i++) {
        int hh = (size * i) / half;
        int y1 = y + (size - hh) / 2;
        u->drawVLine(x + i, y1, hh);
        u->drawVLine(x + half + i, y1, hh);
    }
    u->drawVLine(x + size - 1, y, size);
    return size;
}

uint8_t sym_prev(U8G2* u, int x, int y, uint8_t size) {
    int half = size / 2;
    u->drawVLine(x, y, size);
    for (int i = 0; i < half; i++) {
        int hh = (size * (half - i)) / half;
        int y1 = y + (size - hh) / 2;
        u->drawVLine(x + 1 + i, y1, hh);
        u->drawVLine(x + 1 + half + i, y1, hh);
    }
    return size;
}

//  СТРЕЛКИ

uint8_t sym_arrow_up(U8G2* u, int x, int y, uint8_t size) {
    int w = size;
    int h = size;
    int cx = x + w / 2;
    u->drawVLine(cx, y + 2, h - 2);
    for (int i = 0; i < w / 2; i++) {
        u->drawPixel(cx - i, y + i);
        u->drawPixel(cx + i, y + i);
    }
    return w;
}

uint8_t sym_arrow_down(U8G2* u, int x, int y, uint8_t size) {
    int w = size;
    int h = size;
    int cx = x + w / 2;
    u->drawVLine(cx, y, h - 2);
    for (int i = 0; i < w / 2; i++) {
        u->drawPixel(cx - i, y + h - 1 - i);
        u->drawPixel(cx + i, y + h - 1 - i);
    }
    return w;
}

uint8_t sym_arrow_left(U8G2* u, int x, int y, uint8_t size) {
    int w = size;
    int h = size;
    int cy = y + h / 2;
    u->drawHLine(x + 2, cy, w - 2);
    for (int i = 0; i < h / 2; i++) {
        u->drawPixel(x + i, cy - i);
        u->drawPixel(x + i, cy + i);
    }
    return w;
}

uint8_t sym_arrow_right(U8G2* u, int x, int y, uint8_t size) {
    int w = size;
    int h = size;
    int cy = y + h / 2;
    u->drawHLine(x, cy, w - 2);
    for (int i = 0; i < h / 2; i++) {
        u->drawPixel(x + w - 1 - i, cy - i);
        u->drawPixel(x + w - 1 - i, cy + i);
    }
    return w;
}

//  СПЕЦСИМВОЛЫ

uint8_t sym_heart(U8G2* u, int x, int y, uint8_t size) {
    int w = size;
    int h = size;
    int r = w / 4;
    if (r < 1) r = 1;
    u->drawDisc(x + r, y + r, r);
    u->drawDisc(x + w - r - 1, y + r, r);
    for (int i = 0; i <= h / 2; i++) {
        int y1 = y + h - 1 - i;
        int x1 = x + i;
        int x2 = x + w - 1 - i;
        if (x1 <= x2) u->drawHLine(x1, y1, x2 - x1 + 1);
    }
    return w;
}

uint8_t sym_ruble(U8G2* u, int x, int y, uint8_t size) {
    int w = (size * 3) / 4;
    u->drawVLine(x, y, size);
    int loopH = size / 2 + 1;
    u->drawHLine(x + 1, y, w - 2);
    u->drawVLine(x + w - 1, y + 1, loopH - 1);
    u->drawHLine(x + 1, y + loopH, w - 2);
    u->drawHLine(x, y + size - 2, w + 1);
    u->drawHLine(x, y + size - 4, w + 1);
    return w + 1;
}

uint8_t sym_numero(U8G2* u, int x, int y, uint8_t size) {
    int w = size;
    int nW = (size * 2) / 3;
    u->drawVLine(x, y, size);
    u->drawVLine(x + nW - 1, y, size);
    for (int i = 0; i < size; i++) {
        u->drawPixel(x + i * nW / size, y + i);
    }
    int ox = x + nW + 1;
    int r = size / 4;
    if (r < 1) r = 1;
    u->drawDisc(ox + r, y + r, r);
    u->setDrawColor(0);
    u->drawDisc(ox + r, y + r, r - 1);
    u->setDrawColor(1);
    return w + 1;
}

uint8_t sym_check(U8G2* u, int x, int y, uint8_t size) {
    int w = (size * 3) / 4;
    int h = size;
    for (int i = 0; i < h / 2; i++) {
        u->drawPixel(x + i, y + h / 2 + i);
        if (i + 1 < h / 2) u->drawPixel(x + i, y + h / 2 + i + 1);
    }
    for (int i = 0; i < h; i++) {
        u->drawPixel(x + h / 2 + i, y + h - 1 - i);
        if (i + 1 < h) u->drawPixel(x + h / 2 + i, y + h - 2 - i);
    }
    return w;
}

uint8_t sym_cross(U8G2* u, int x, int y, uint8_t size) {
    int w = (size * 3) / 4;
    int h = size;
    for (int i = 0; i < h && i < w; i++) {
        u->drawPixel(x + i, y + i);
        u->drawPixel(x + i + 1, y + i);
        u->drawPixel(x + w - 1 - i, y + i);
        u->drawPixel(x + w - 2 - i, y + i);
    }
    return w;
}

uint8_t sym_ellipsis(U8G2* u, int x, int y, uint8_t size) {
    int r = size / 8;
    if (r < 1) r = 1;
    int cy = y + size - r - 1;
    u->drawDisc(x + r,         cy, r);
    u->drawDisc(x + r * 3 + 2, cy, r);
    u->drawDisc(x + r * 5 + 4, cy, r);
    return r * 5 + 4 + r;
}

uint8_t sym_dot(U8G2* u, int x, int y, uint8_t size) {
    int r = size / 2;
    if (r < 1) r = 1;
    u->drawDisc(x + r, y + size / 2, r);
    return r * 2;
}

uint8_t sym_permille(U8G2* u, int x, int y, uint8_t size) {
    // ‰ — три кружка + косая черта
    int r = size / 8;
    if (r < 1) r = 1;
    // Косая
    for (int i = 0; i < size; i++) {
        u->drawPixel(x + size - 3 - i * 2 / size, y + i);
    }
    // Кружки
    u->drawDisc(x + r,         y + r, r);
    u->drawDisc(x + r + 5,     y + r, r);
    u->drawDisc(x + r + 10,    y + r, r);
    return size;
}

uint8_t sym_careof(U8G2* u, int x, int y, uint8_t size) {
    // ℅ — буква c и маленькая o над ней
    int w = (size * 2) / 3;
    // 'c' — полукруг
    int r = w / 2;
    u->drawCircle(x + r, y + size - r - 1, r);
    // 'o' сверху
    int r2 = size / 5;
    if (r2 < 1) r2 = 1;
    u->drawDisc(x + r + r2 + 2, y + r2, r2);
    return w + r2 * 2 + 3;
}

uint8_t sym_empty(U8G2* u, int x, int y, uint8_t size) {
    // ∅ — круг + косая черта
    int r = size / 2;
    if (r < 1) r = 1;
    u->drawCircle(x + r, y + r, r);
    for (int i = 0; i <= r * 2; i++) {
        u->drawPixel(x + i / 2 + 1, y + size - i * size / (2 * r) - 1);
    }
    return size;
}

uint8_t sym_delta(U8G2* u, int x, int y, uint8_t size) {
    // ∆ — треугольник
    int w = (size * 4) / 5;
    int cx = x + w / 2;
    for (int i = 0; i <= w / 2; i++) {
        u->drawPixel(cx - i, y + size - 1);
        u->drawPixel(cx + i, y + size - 1);
    }
    u->drawPixel(cx, y);
    // стороны
    for (int i = 1; i < size; i++) {
        int xl = cx - (w / 2) * i / size;
        int xr = cx + (w / 2) * i / size;
        u->drawPixel(xl, y + i);
        u->drawPixel(xr, y + i);
    }
    return w;
}

uint8_t sym_sqrt(U8G2* u, int x, int y, uint8_t size) {
    int w = (size * 4) / 5;
    int h = size;
    // Начальная палочка
    u->drawPixel(x, y + h / 2);
    u->drawPixel(x + 1, y + h / 2 + 1);
    u->drawPixel(x + 2, y + h - 1);
    u->drawPixel(x + 3, y + h - 1);
    // Верхняя черта
    for (int i = 4; i < w; i++) {
        u->drawPixel(x + i, y);
    }
    return w;
}

uint8_t sym_omega(U8G2* u, int x, int y, uint8_t size) {
    int w = (size * 4) / 5;
    int h = size;
    int cx = x + w / 2;
    // Круглая часть
    int r = w / 2 - 1;
    u->drawCircle(cx, y + h - r - 1, r);
    // Основание
    u->drawHLine(cx - r, y + h - 1, r * 2);
    return w;
}

uint8_t sym_pi_cap(U8G2* u, int x, int y, uint8_t size) {
    int w = (size * 4) / 5;
    int h = size;
    int legW = w / 4;
    u->drawHLine(x, y, w);
    u->drawVLine(x, y, h);
    u->drawVLine(x + w - 1, y, h);
    return w;
}

uint8_t sym_mu(U8G2* u, int x, int y, uint8_t size) {
    int w = (size * 4) / 5;
    int h = size;
    u->drawVLine(x, y, h - 2);
    u->drawVLine(x + w - 1, y, h - 2);
    u->drawVLine(x + w - 1, y + h - 2, 3);
    // Низ
    for (int i = 0; i < w - 1; i++) {
        u->drawPixel(x + i, y + h - 3);
    }
    return w;
}

uint8_t sym_pi(U8G2* u, int x, int y, uint8_t size) {
    int w = (size * 4) / 5;
    u->drawHLine(x, y, w);
    u->drawVLine(x + 2, y + 1, size - 1);
    u->drawVLine(x + w - 3, y + 1, size - 1);
    return w;
}

uint8_t sym_neq(U8G2* u, int x, int y, uint8_t size) {
    int w = size;
    u->drawHLine(x, y + size / 3, w);
    u->drawHLine(x, y + size * 2 / 3, w);
    // Косая
    for (int i = 0; i < size; i++) {
        u->drawPixel(x + i, y + size - 1 - i);
    }
    return w;
}

uint8_t sym_approx(U8G2* u, int x, int y, uint8_t size) {
    int w = size;
    int cy = y + size / 3;
    // Две волны
    for (int i = 0; i < w; i++) {
        u->drawPixel(x + i, cy - (i < w / 2 ? 1 : -1));
        u->drawPixel(x + i, cy + size / 3 - (i < w / 2 ? 1 : -1));
    }
    return w;
}

uint8_t sym_infinity(U8G2* u, int x, int y, uint8_t size) {
    int w = size;
    int cy = y + size / 2;
    int r = size / 4;
    if (r < 1) r = 1;
    u->drawCircle(x + r, cy, r);
    u->drawCircle(x + w - r - 1, cy, r);
    return w;
}

uint8_t sym_prime(U8G2* u, int x, int y, uint8_t size) {
    int h = size / 2;
    for (int i = 0; i < h; i++) {
        u->drawPixel(x + 1, y + i);
    }
    return 3;
}

uint8_t sym_dprime(U8G2* u, int x, int y, uint8_t size) {
    int h = size / 2;
    for (int i = 0; i < h; i++) {
        u->drawPixel(x + 1, y + i);
        u->drawPixel(x + 4, y + i);
    }
    return 6;
}

uint8_t sym_cent(U8G2* u, int x, int y, uint8_t size) {
    // ¢ — как C с чертой
    int w = (size * 3) / 4;
    int cx = x + w / 2;
    int cy = y + size / 2;
    int r = size / 3;
    u->drawCircle(cx, cy, r);
    // Открытая правая сторона
    u->setDrawColor(0);
    for (int i = cy - r + 1; i < cy + r; i++) {
        u->drawPixel(cx + r, i);
    }
    u->setDrawColor(1);
    // Черта
    u->drawVLine(cx, y, size);
    return w;
}

//  ДРОБИ
uint8_t sym_fraction(U8G2* u, int x, int y, uint8_t size,
                     char num, char den) {
    int w = size;
    int half = size / 2;

    // Верхняя цифра
    char buf[2] = { num, 0 };
    u->setFont(u8g2_font_5x7_t_cyrillic);
    u->drawStr(x + 1, y + half - 1, buf);

    // Нижняя цифра
    buf[0] = den;
    u->drawStr(x + 1, y + size, buf);

    // Косая черта (диагональ)
    for (int i = 0; i < size; i++) {
        u->drawPixel(x + w - 2 - i * (w - 2) / size, y + i);
        u->drawPixel(x + w - 3 - i * (w - 2) / size, y + i);
    }
    return w;
}

//  ВЕРХНИЕ ИНДЕКСЫ
uint8_t sym_superscript(U8G2* u, int x, int y, uint8_t size, char ch) {
    // Уменьшенный шрифт для верхнего индекса
    u->setFont(u8g2_font_4x6_tf);           // 4×6 без кириллицы (для ⁰¹²³... и ⁿ)
    char buf[2] = { ch, 0 };
    u->drawStr(x, y + 5, buf);
    return 5;
}

//  ГЛАВНАЯ ФУНКЦИЯ — парсер строки
int draw_string_with_symbols(U8G2* u, int x, int y, const char* str) {
    int startX = x;
    int curX = x;
    int cy = y;             // базовая линия

    // Запоминаем текущий шрифт (для восстановления)
    const uint8_t* origFont = u->getU8g2()->font;

    while (*str) {
        uint8_t c = (uint8_t)*str;

        // Управляющие токены 0x01–0x1F
        if (c >= 0x01 && c <= 0x1F) {
            int symSize = u->getFontAscent();
            int topY = cy - symSize + 1;
            uint8_t w = 0;

            switch (c) {
                case 0x01: w = sym_play(u, curX, topY, symSize); break;
                case 0x02: w = sym_pause(u, curX, topY, symSize); break;
                case 0x03: w = sym_stop(u, curX, topY, symSize); break;
                case 0x04: w = sym_next(u, curX, topY, symSize); break;
                case 0x05: w = sym_prev(u, curX, topY, symSize); break;
                case 0x06: w = sym_heart(u, curX, topY, symSize); break;
                case 0x07: w = sym_ruble(u, curX, topY, symSize); break;
                case 0x08: w = sym_numero(u, curX, topY, symSize); break;
                case 0x09: w = sym_check(u, curX, topY, symSize); break;
                case 0x0A: w = sym_cross(u, curX, topY, symSize); break;
                case 0x0B: w = sym_ellipsis(u, curX, topY, symSize); break;
                case 0x0C: w = sym_dot(u, curX, topY, symSize); break;
                case 0x0D: w = sym_arrow_up(u, curX, topY, symSize); break;
                case 0x0E: w = sym_arrow_down(u, curX, topY, symSize); break;
                case 0x0F: w = sym_arrow_left(u, curX, topY, symSize); break;
                case 0x10: w = sym_arrow_right(u, curX, topY, symSize); break;
                case 0x11: w = sym_permille(u, curX, topY, symSize); break;
                case 0x12: w = sym_careof(u, curX, topY, symSize); break;
                case 0x13: w = sym_empty(u, curX, topY, symSize); break;
                case 0x14: w = sym_delta(u, curX, topY, symSize); break;
                case 0x15: w = sym_sqrt(u, curX, topY, symSize); break;
                case 0x16: w = sym_omega(u, curX, topY, symSize); break;
                case 0x17: w = sym_pi_cap(u, curX, topY, symSize); break;
                case 0x18: w = sym_mu(u, curX, topY, symSize); break;
                case 0x19: w = sym_pi(u, curX, topY, symSize); break;
                case 0x1A: w = sym_neq(u, curX, topY, symSize); break;
                case 0x1B: w = sym_approx(u, curX, topY, symSize); break;
                case 0x1C: w = sym_infinity(u, curX, topY, symSize); break;
                case 0x1D: w = sym_prime(u, curX, topY, symSize); break;
                case 0x1E: w = sym_dprime(u, curX, topY, symSize); break;
                case 0x1F: w = sym_cent(u, curX, topY, symSize); break;
            }
            curX += w + 1;
            str++;
            continue;
        }

        // Модификатор '~'
        // '~X' где X:
        //   '0'..'9' → ⁰..⁹
        //   'n' → ⁿ
        //   'h' → ½, 'q' → ¼, 't' → ⅓, 'f' → ⅕,
        //   's' → ⅙, 'e' → ⅛, 'g' → ⅔, 'r' → ¾,
        //   'a' → ⅓ (треть), 'b' → ⅔, 'c' → ⅜, 'd' → ⅝
        if (c == '~' && *(str + 1)) {
            uint8_t nx = (uint8_t)*(str + 1);
            int symSize = u->getFontAscent();
            int topY = cy - symSize + 1;
            uint8_t w = 0;

            // Верхние индексы: 0-9, n
            if (nx >= '0' && nx <= '9') {
                w = sym_superscript(u, curX, topY, symSize, nx);
                curX += w + 1;
                str += 2;
                continue;
            }
            if (nx == 'n') {
                w = sym_superscript(u, curX, topY, symSize, 'n');
                curX += w + 1;
                str += 2;
                continue;
            }

            // Дроби (двухбайтовые)
            char num = 0, den = 0;
            switch (nx) {
                case 'h': num = '1'; den = '2'; break;  // ½
                case 'q': num = '1'; den = '4'; break;  // ¼
                case 't': num = '1'; den = '3'; break;  // ⅓
                case 'f': num = '1'; den = '5'; break;  // ⅕
                case 's': num = '1'; den = '6'; break;  // ⅙
                case 'e': num = '1'; den = '8'; break;  // ⅛
                case 'g': num = '2'; den = '3'; break;  // ⅔
                case 'r': num = '3'; den = '4'; break;  // ¾
                case 'b': num = '2'; den = '5'; break;  // ⅖
                case 'c': num = '3'; den = '8'; break;  // ⅜
                case 'd': num = '5'; den = '8'; break;  // ⅝
                case 'l': num = '3'; den = '5'; break;  // ⅗
                case 'm': num = '4'; den = '5'; break;  // ⅘
                case 'o': num = '5'; den = '6'; break;  // ⅚
                case 'p': num = '7'; den = '8'; break;  // ⅞
                case 'w': num = '1'; den = '7'; break;  // ⅐
                case 'x': num = '1'; den = '9'; break;  // ⅑
                case 'y': num = '1'; den = '1'; break;  // ⅒ (1/10)
            }
            if (num && den) {
                w = sym_fraction(u, curX, topY, symSize, num, den);
                curX += w + 1;
                str += 2;
                continue;
            }
        }

        // Обычный символ (UTF-8)
        // Определяем длину UTF-8 последовательности
        int bytes = 1;
        if      ((c & 0x80) == 0x00) bytes = 1;
        else if ((c & 0xE0) == 0xC0) bytes = 2;
        else if ((c & 0xF0) == 0xE0) bytes = 3;
        else if ((c & 0xF8) == 0xF0) bytes = 4;

        // Копируем символ во временный буфер
        char buf[8] = { 0 };
        for (int i = 0; i < bytes; i++) buf[i] = str[i];

        // Восстанавливаем исходный шрифт (он мог быть изменён в sym_superscript)
        u->setFont(origFont);

        u->drawStr(curX, cy, buf);
        curX += u->getStrWidth(buf);

        str += bytes;
    }

    // Восстановить исходный шрифт
    u->setFont(origFont);

    return curX - startX;
}

//  Регион-код — просто две заглавные буквы
uint8_t draw_region_code(U8G2* u, int x, int y, const char* code) {
    if (!code || !code[0]) return 0;

    // Только 2 символа, верхний регистр
    char buf[3] = { 0 };
    for (int i = 0; i < 2 && code[i]; i++) {
        char c = code[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        buf[i] = c;
    }

    u->drawStr(x, y, buf);
    return u->getStrWidth(buf);
}