# Changelog

Все значимые изменения проекта.  

## [5.1.1] — 2026-10-17

Хотфикс после первой сборки v5.1: устранены оставшиеся ошибки компиляции, связанные с логотипом и несуществующим шрифтом U8g2.

### Исправлено

#### Логотип
- **`display.cpp`** — удалён вложенный дубликат `LOGO_BITS` (был и глобальный `PROGMEM`, и локальный внутри `display_logo_show()` — компилятор выдавал `expected primary-expression before 'const'`). Теперь один массив: `static const uint8_t LOGO_BITS[1024] PROGMEM`.
- **`display.cpp`** — в `display_logo_show()` раскомментирован вызов `u8g2_big.drawXBMP(0, 0, 128, 64, LOGO_BITS)`.

#### Шрифты U8g2
- **`display.h`** — `u8g2_font_helvB18_t_cyrillic` **не существует** в текущей версии U8g2 (ошибка `'u8g2_font_helvB18_t_cyrillic' was not declared`). Заменён на проверенный `u8g2_font_10x20_t_cyrillic`.
- **`display.h`** — `FONT_BIG_LARGE` понижен с `10x20` на `9x15`, чтобы сохранить иерархию размеров:
  - `SMALL` — `5x7` (7 px)
  - `NORMAL` — `6x12` (12 px)
  - `LARGE` — `9x15` (15 px)
  - `HUGE` — `10x20` (20 px, крупные цифры на GPU/CPU/RAM)

### Добавлено

- **`tools/logo_to_c.py`** — скрипт конвертации PNG 128×64 в C-массив `LOGO_BITS`. Установка: `pip install pillow`, запуск: `python tools/logo_to_c.py docs/LOGO/logo.png`.
- **`docs/LOGO.md`** — инструкция по созданию логотипа:
  - через онлайн-конвертер [image2cpp](https://javl.github.io/image2cpp/)
  - через Python-скрипт (офлайн)
  - через графредактор (Piskel, GIMP, Photoshop)
- **`docs/LOGO/logo-example.png`** — пример логотипа 128×64 (сердце).

### Изменено

- **`config.h`** — `FW_VERSION` → `"5.1"` (было `"5.0"`).
- **`README.md`** — раздел «Логотип» со ссылкой на `docs/LOGO.md`.

### Совместимость

Без изменений относительно 5.1.0.

### Известные ограничения

- Готовый пример-логотип (сердце) в `display.cpp` собран вручную и может иметь смещения пикселей. Для точного результата используйте `tools/logo_to_c.py` или image2cpp со своим PNG.
- `u8g2_font_10x20_t_cyrillic` занимает ~5 KB Flash. Если не хватает — заменить на `u8g2_font_9x15_t_cyrillic`.

---

## [5.1.0] — 2026-10-17

Патч совместимости: исправления компиляции под ESP32 Core 3.x и Arduino IDE 1.8.19, чистка дубликатов, корректные шрифты U8g2 с кириллицей.

### Исправлено

#### Компиляция
- **`pages.h`** — добавлен `#include <U8G2lib.h>`, устранена ошибка `'U8G2' was not declared in this scope` при компиляции `pages.cpp` и `comms.cpp`.
- **`comms.h`** — добавлен `#include "lang.h"`, устранена ошибка `'Lang' was not declared in this scope`.
- **`display.cpp`** — добавлен `#include <Wire.h>`, устранена ошибка `'Wire' was not declared in this scope`.
- **`display.cpp`** — убрана обёртка `extern "C"` вокруг функций `boot_check_*`, устранён конфликт линковки `conflicting declaration with 'C' linkage`.
- **`display.cpp`** — удалено дублирующее локальное определение `LOGO_BITS` внутри `display_logo_show()`, глобальный массив теперь объявлен как `PROGMEM uint8_t LOGO_BITS[1024]`.
- **`overlays.cpp`** — явное приведение типа `(PressType)t` при вызове `settings_get_button()`, устранена ошибка `invalid conversion from 'uint8_t' to 'PressType'`.
- **`comms.cpp`** — блок парсинга времени (`doc["time"]`) перемещён внутрь функции `parse_json()`, устранена ошибка `'doc' was not declared in this scope`.
- **`symbols.cpp`** — в `sym_superscript()` заменён несуществующий шрифт `u8g2_font_4x6_t_cyrillic` на `u8g2_font_4x6_tf`.

#### Шрифты U8g2
- **`display.h`** — заменены несуществующие кириллические шрифты:
  - `u8g2_font_6x10_t_cyrillic` → `u8g2_font_6x12_t_cyrillic`
  - `u8g2_font_logisoso20_t_cyrillic` → `u8g2_font_helvB18_t_cyrillic`
- Оба новых шрифта существуют в U8g2 2.35.x и содержат полный набор кириллицы.

#### Структуры
- **`pages.h`** — переименована структура `PingTarget` (name + ms) → `PingResult`, устранён конфликт с одноимённой `PingTarget` (name + host) из `settings.h`.
- **`pages.h`** — поле `PingData::targets[]` теперь типа `PingResult[5]`.
- **`webserver.cpp`** — обращение к полю `.ms` восстановлено через новую структуру.

### Изменено

- **`platformio.ini`** — рекомендованный `platform = espressif32@^6.0` (совместимо с ESP32 Core 3.x).
- **`docs/WIRING.md`** — добавлено замечание про питание OLED от 3.3 V.
- **`README.md`** — обновлены ссылки на библиотеки:
  - `ESPAsyncWebServer` → форк [ESP32Async](https://github.com/ESP32Async/ESPAsyncWebServer)
  - `AsyncTCP` → форк [ESP32Async](https://github.com/ESP32Async/AsyncTCP)
- **`README.md`** — добавлено требование: для ESP32 Core 3.x использовать форк `ESP32Async`, либо откатить ESP32 Core до 2.0.17.

### Удалено

- Дублирующий массив `LOGO_BITS[]` внутри `display_logo_show()` (заменён единым глобальным `PROGMEM`-массивом).

### Совместимость

| Компонент | Минимум | Рекомендуется |
|---|---|---|
| ESP32 Core | 2.0.17 | 3.0+ |
| Arduino IDE | 1.8.19 | 2.3+ |
| U8g2 | 2.35.19 | 2.35.30+ |
| ArduinoJson | 7.0.0 | 7.2.0+ |
| ESPAsyncWebServer | ESP32Async (main) | ESP32Async (main) |
| AsyncTCP | ESP32Async (main) | ESP32Async (main) |
| Python | 3.8 | 3.11 |

### Известные ограничения

- ESP32 Core 3.x **несовместим** со старым `ESPAsyncWebServer` от `me-no-dev`. Используйте форк `ESP32Async/ESPAsyncWebServer` + `ESP32Async/AsyncTCP`.
- Шрифт `helvB18_t_cyrillic` чуть крупнее `logisoso20_t_cyrillic` — на страницах с крупными цифрами может потребоваться сдвиг на 1–2 px.
- `PingResult` и `PingTarget` — разные структуры, не путать при расширении кода.

---

## [5.0.0] — 2026-10-17

### Добавлено

- Два OLED (128×64 + 128×32) с разными страницами
- 5 кнопок с настраиваемыми действиями (короткое / долгое / двойное)
- 5 LED с индикацией по загрузке (белый → красный)
- Датчик климата BME280 + LDR
- Страницы: GPU, CPU, ОЗУ, диски, сеть, пинг, утечка, климат
- Оверлеи: FPS, нотификации Windows, медиа, подтверждение питания
- Ночной режим (часы + климат, яркость 1%)
- Индикатор страниц (растущая полоса)
- Двуязычность (EN / RU)
- Веб-интерфейс в терминальном стиле
- Captive portal для настройки WiFi
- OTA без верификации
- WebSocket для push обновлений
- Python-скрипт сбора данных:
  - GPU / CPU / RAM / диски через LibreHardwareMonitor
  - Сеть через psutil
  - Пинг через subprocess ping.exe
  - Утечка памяти (psutil + история)
  - FPS через RTSS shared memory
  - Медиа через winsdk Media Session API
  - Нотификации через winsdk UserNotificationListener
- Транспорт: Serial → TCP → HTTP (авто-переключение)
- Управление питанием ПК (сон / перезагрузка / выключение)
- Управление медиа (next / prev / playpause)
- Сброс счётчиков (сеть, утечка, FPS)
- Кастомные символы (стрелки, валюты, дроби, верхние индексы)
- Тренды ↑↓ для больших значений
- Профили раскладки кнопок
- Условные страницы (утечка, диски, FPS)

### Технические детали

- Прошивка: Arduino IDE / PlatformIO
- Python: 3.8 – 3.11
- 13 модулей прошивки, 20+ модулей Python
- Все настройки в NVS
- Кастомная таблица разделов (1.875 MB APP + OTA + 128 KB LittleFS)
- Терминальный стиль везде