# PC Monitor v.5.1.1

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-5.0.0-brightgreen.svg)](CHANGELOG.md)
[![Platform](https://img.shields.io/badge/platform-ESP32-blue.svg)](https://www.espressif.com/)
[![Python](https://img.shields.io/badge/python-3.8%20%E2%80%93%203.11-blue.svg)](https://www.python.org/)

Мониторинг ПК на ESP32 с двумя OLED-дисплеями, 5 кнопками, 5 LED и веб-интерфейсом в терминальном стиле.

Проект собирает телеметрию с ПК (CPU, GPU, RAM, диски, сеть, пинг, утечки памяти, FPS, медиа, нотификации Windows) и выводит её на два OLED-дисплея. Управление — 5 кнопок, веб-интерфейс и captive portal для первой настройки WiFi.

---

## Возможности

### Датчики (с ПК)

- **GPU:** загрузка, температура, VRAM, питание, кулер, драйвер — NVIDIA / AMD / Intel
- **CPU:** загрузка, температура, частота, питание, Vcore, кулер, core max
- **RAM:** used/total, частота, тип DDR, температура, напряжение, каналы
- **Диски:** NVMe / SATA / HDD — занято, температура, скорости R/W, ресурс SSD, активность
- **Сеть:** скорости ↓/↑, загрузка канала, IP, всего скачано/отдано
- **Пинг:** до 5 целей (Google, Яндекс, роутер, Cloudflare, Quad9)
- **Утечка памяти:** топ процессов с приростом > 100 MB за сессию
- **FPS:** через RTSS (любые игры)
- **Медиа:** Spotify, YouTube, VLC, Chrome — что играет сейчас
- **Нотификации Windows:** оверлей на большом OLED

### С ESP32

- **Климат:** BME280 (температура, влажность, давление) + LDR (освещённость)
- **Часы:** время с ПК + дата + день недели

### Интерфейс

- **2× OLED:** 128×64 (основной) + 128×32 (дополнительный)
- **5 кнопок:** полностью настраиваемые (короткое / долгое / двойное)
- **5 LED:** индикация по загрузке (белый → синий → зелёный → жёлтый → красный)
- **8 страниц** с индикатором прогресса внизу
- **Оверлеи:** FPS, нотификации, медиа, подтверждение питания
- **Ночной режим:** часы + климат, яркость 1%
- **Веб-интерфейс:** терминальный стиль (зелёный на чёрном), адаптив
- **Captive portal:** настройка WiFi при первом запуске
- **OTA:** обновление прошивки через браузер
- **Двуязычность:** EN / RU

---

## Железо

| Компонент | Модель | Кол-во |
|---|---|---|
| Микроконтроллер | ESP32 DevKit | 1 |
| OLED 128×64 | SSD1306 I2C | 1 |
| OLED 128×32 | SSD1306 I2C | 1 |
| Датчик климата | BME280 | 1 |
| Фоторезистор | LDR + 10 kΩ | 1 |
| Кнопки | тактовые | 5 |
| LED | 5 цветов (белый, синий, зелёный, жёлтый, красный) | 5 |
| Резисторы | 220–330 Ω | 5 |

Схема подключения: [docs/WIRING.md](docs/WIRING.md)  
Распиновка: [docs/PINS.md](docs/PINS.md)

---

## Установка

### Прошивка ESP32 (Arduino IDE)

**1. Установи [Arduino IDE 2.x](https://www.arduino.cc/en/software) и настрой ESP32 Board Manager по [инструкции](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html).**

**2. Установи библиотеки** (через Library Manager или по ссылкам):

- [U8g2](https://github.com/olikraus/U8g2_Arduino/releases)
- [Adafruit BME280 Library](https://github.com/adafruit/Adafruit_BME280_Library/releases)
- [Adafruit Unified Sensor](https://github.com/adafruit/Adafruit_Sensor/releases)
- [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer)
- [AsyncTCP](https://github.com/ESP32Async/AsyncTCP)

**3. Установи плагин [arduino-littlefs-upload](https://github.com/earlephilhower/arduino-littlefs-upload/releases)** для загрузки веб-файлов в LittleFS:

- Скачай `.vsix` из раздела Releases.
- Распакуй в:
  - Linux: `~/.arduino15/tools/`
  - Windows: `%LOCALAPPDATA%\Arduino15\tools\`

**4. Настройки платы:**

| Параметр | Значение |
|---|---|
| Board | ESP32 Dev Module |
| Partition Scheme | Minimal SPIFFS (Large APP) |
| Flash Size | 4 MB |
| Upload Speed | 921600 |

**5. Открой `firmware/PC_Monitor.ino`, загрузи прошивку.**

**6. Загрузи веб-файлы: Tools → ESP32 LittleFS Data Upload.**

### Прошивка ESP32 (PlatformIO)

```bash
cd firmware
pio run -t upload      # прошивка
pio run -t uploadfs    # веб-файлы
pio device monitor     # serial monitor
```

### Python на ПК

**1. Установи [Python 3.11](https://www.python.org/downloads/release/python-3119/) (рекомендуемая версия).**

**2. Установи зависимости:**

```bash
cd pc
python -m venv venv
venv\Scripts\activate   # Windows
pip install -r requirements.txt
python main.py
```

**Дополнительное ПО:**

- **RTSS (RivaTuner Statistics Server)** — для сбора FPS: [скачать с Guru3D](https://www.guru3d.com/files-details/rtss-rivatuner-statistics-server-download.html)
- **LibreHardwareMonitorLib.dll** — для датчиков GPU/CPU: [скачать с GitHub](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases) (нужна только DLL, положи её рядом со скриптом `main.py`)

**Требования:**

- Windows 10 (1809+) или Windows 11
- Python 3.8 – 3.11 (winsdk не работает на 3.12+)

---

## Первый запуск

1. **Прошей ESP32.** После старта — экран инициализации, затем логотип.
2. **Если WiFi не настроен** — ESP переходит в AP-режим:
   - SSID: `PCMonitor`
   - Пароль: `12345678`
   - Открой `http://192.168.4.1` — выбери сеть, введи пароль.
3. **Запусти Python** — он найдёт ESP по COM / mDNS / TCP.
4. **Открой веб-интерфейс** по IP ESP.

---

## Интерфейс

### Страницы OLED

| # | Страница | LED |
|---|---|---|
| 1 | GPU | загрузка GPU (накопительно) |
| 2 | CPU | загрузка CPU |
| 3 | ОЗУ | использование ОЗУ |
| 4 | Диски | активность диска |
| 5 | Сеть | загрузка канала |
| 6 | Пинг | один LED по avg |
| 7 | Утечка (условная) | один LED по макс. приросту |
| 8 | Климат | один LED по температуре |

### Оверлеи

- **FPS** — рамка по краю большого OLED, LED выключены
- **Нотификации** — инверсия на большом, 5 сек
- **Медиа** — инверсия на малом, +10 сек после паузы
- **Подтверждение питания** — малый OLED, 5 сек

Подробнее: [docs/OLED.md](docs/OLED.md)

### Веб

Dashboard · Settings · Control · Pages · Buttons · OTA · Logs

Стиль: терминал (зелёный на чёрном), адаптив 1 / 2 / 3 колонки.

---

## REST API

| Метод | Путь | Описание |
|---|---|---|
| GET | `/api/status` | Статус ESP32 |
| GET | `/api/data` | Все датчики |
| POST | `/api/data` | Приём данных с ПК (fallback) |
| GET/POST | `/api/settings` | Настройки |
| POST | `/api/control` | Действия |
| GET/POST | `/api/pages` | Порядок страниц |
| GET/POST | `/api/buttons` | Раскладка кнопок |
| GET | `/api/logs` | Логи |
| GET | `/api/wifi/scan` | Скан WiFi |
| POST | `/api/ota` | OTA |

WebSocket: `ws://<ip>/ws`

Подробнее: [docs/API.md](docs/API.md)

---

## Разработка

```bash
# Сборка прошивки
cd firmware && pio run

# Запуск Python с подробными логами
cd pc && python -u main.py
```

Включи `DEBUG = True` в `pc/config.py` для подробного вывода.

---

## Лицензия

MIT — см. [LICENSE](LICENSE).

---

## Автор

**KTH-Studio** ♥
