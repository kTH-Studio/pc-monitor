# REST API + WebSocket

## Базовый URL

```
http://<esp-ip>/
```

## REST API

### `GET /api/status`

Статус ESP32.

```json
{
  "fw": "5.0",
  "name": "PC Monitor",
  "author": "KTH-Studio",
  "uptime": 12345,
  "heap": 145632,
  "rssi": -58,
  "ip": "192.168.1.42",
  "ap": false,
  "ssid": "HomeWiFi",
  "lang": 1,
  "linked": true,
  "channel": 1,
  "page": 0,
  "pages": 8,
  "night": 0,
  "flash_size": 4194304,
  "cpu_freq": 240
}
```

### `GET /api/data`

Все датчики.

```json
{
  "gpu": { "present": true, "name": "RTX 4070 Ti", "load": 47, "temp": 62, "vram_used": 4.2, "vram_total": 12, "power": 180, "fan": 1450, "driver": "552.22" },
  "cpu": { "present": true, "name": "Ryzen 7 5800X3D", "load": 34, "temp": 58, "freq": 4450, "power": 65, "core_max": 64, "fan": 1250, "vcore": 1250 },
  "ram": { "present": true, "name": "Kingston Fury", "load": 47, "used": 15.2, "total": 32, "freq": 3200, "type": "DDR4", "temp": 42, "voltage": 1350, "dual": true },
  "disks": [ { "letter": "C", "name": "Samsung 980 Pro", "load": 64, "temp": 48, "used": 245, "total": 500, "max_speed": 1200, "type": "NVMe", "read_rate": 540, "write_rate": 320, "life_left": 98, "activity": 12 } ],
  "net": { "present": true, "adapter": "Intel I225-V", "down_mbit": 12.4, "up_mbit": 3.2, "total_down_gb": 245.8, "total_up_gb": 89.3, "ip": "192.168.1.42", "link_speed": 1000 },
  "ping": { "present": true, "avg": 6, "count": 5, "targets": [ { "name": "Google", "ms": 8 }, { "name": "Яндекс", "ms": 1 } ] },
  "leak": { "present": false, "max_delta_mb": 0, "items": [] },
  "fps": { "present": false },
  "media": { "present": false },
  "climate": { "temp": 24.3, "hum": 48, "press_mm": 760, "light": 85 }
}
```

### `POST /api/data`

Приём данных с ПК (fallback через HTTP). Тело — тот же JSON.

### `GET /api/settings`

Все настройки.

### `POST /api/settings`

Обновление настроек.

```json
{
  "lang": 1,
  "brightness": 100,
  "night_bright": 1,
  "night_mode": 0,
  "night_on": 23,
  "night_off": 7,
  "notify_ms": 5000,
  "media_ms": 10000,
  "power_ms": 5000,
  "media_prio": "Spotify;YouTube;VLC",
  "thresholds": {
    "gpu_temp": 85, "cpu_temp": 90,
    "ram_pct": 90, "disk_pct": 90,
    "ping_ms": 200, "leak_mb": 500
  },
  "ping": [
    { "name": "Google", "host": "8.8.8.8" },
    { "name": "Яндекс", "host": "77.88.55.88" }
  ],
  "wifi": { "ssid": "HomeWiFi", "pass": "password123" },
  "conditions": { "leak": "auto", "disks": "auto", "fps": "auto" }
}
```

### `POST /api/control`

Действия.

```json
{ "action": "page_next" }
{ "action": "page_prev" }
{ "action": "page_set", "value": 3 }
{ "action": "night_toggle" }
{ "action": "brightness", "value": 50 }
{ "action": "power_sleep" }
{ "action": "power_reboot" }
{ "action": "power_off" }
{ "action": "power_cancel" }
{ "action": "media_next" }
{ "action": "media_prev" }
{ "action": "media_playpause" }
{ "action": "esp_restart" }
{ "action": "esp_reset" }
{ "action": "wifi_clear" }
{ "action": "reset_counters" }
{ "action": "reset_net" }
{ "action": "reset_leak" }
{ "action": "reset_fps" }
{ "action": "reset_all" }
```

### `GET/POST /api/pages`

Порядок страниц.

```json
{
  "order": [0, 1, 2, 3, 4, 5, 6, 7],
  "count": 8
}
```

### `GET/POST /api/buttons`

Раскладка кнопок.

```json
{
  "buttons": [
    { "short": 2, "long": 0, "double": 0 },
    { "short": 5, "long": 0, "double": 0 },
    { "short": 3, "long": 0, "double": 0 },
    { "short": 1, "long": 0, "double": 11 },
    { "short": 6, "long": 7, "double": 0 }
  ]
}
```

### `GET /api/logs`

Массив строк.

```json
["12345: [SYS] boot", "12890: [WIFI] STA OK"]
```

### `GET /api/wifi/scan`

```json
{
  "networks": [
    { "ssid": "HomeWiFi", "rssi": -45, "enc": true },
    { "ssid": "Guest", "rssi": -72, "enc": false }
  ]
}
```

### `POST /api/ota`

Multipart с полем `firmware` (файл `.bin`).

## WebSocket

`ws://<esp-ip>/ws`

### От ESP к клиенту

```json
{ "type": "state", "page": 0, "pages": 8, "linked": true, "rssi": -58, "heap": 145632 }
{ "type": "log", "line": "[COMMS] TCP client connected" }
{ "type": "notify", "text": "Telegram: Иван" }
```

### От клиента к ESP

```json
{ "type": "hello" }
```

## Обратный канал (ESP → Python)

По Serial или TCP:

```json
{ "cmd": "power", "action": "off" }
{ "cmd": "media", "action": "next" }
{ "cmd": "lang", "value": 1 }
{ "cmd": "reset", "what": "counters" }
```

## Коды ответов

| Код | Значение |
|---|---|
| 200 | OK |
| 400 | Неверный JSON |
| 404 | Не найдено |
| 500 | Ошибка сервера |