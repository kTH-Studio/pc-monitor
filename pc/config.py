#  PC Monitor v.5.0 — Python
#  by KTH-Studio

# ESP32
ESP_COM_PORT      = "COM3"        # для Serial (подставь свой)
ESP_BAUD          = 115200
ESP_TCP_HOST      = ""            # IP ESP32 (если пусто — пробуем автоопределить)
ESP_TCP_PORT      = 8888
ESP_HTTP_HOST     = ""            # IP ESP32 (для POST /api/data)
ESP_HTTP_PORT     = 80

# Приоритет каналов: Serial → TCP → HTTP
LINK_PRIORITY     = ["serial", "tcp", "http"]
LINK_RETRY_SEC    = 5             # если канал отвалился — пробуем следующий

# Интервалы опроса (сек)
INTERVAL_GPU      = 1.0
INTERVAL_CPU      = 1.0
INTERVAL_RAM      = 1.0
INTERVAL_DISKS    = 2.0
INTERVAL_NET      = 1.0
INTERVAL_PING     = 2.0
INTERVAL_LEAK     = 5.0
INTERVAL_FPS      = 1.0
INTERVAL_MEDIA    = 2.0
INTERVAL_NOTIFY   = 3.0
INTERVAL_SEND     = 1.0           # отправка пакета на ESP

# Пинг
PING_TARGETS = [
    {"name": "Google",     "host": "8.8.8.8"},
    {"name": "Яндекс",     "host": "77.88.55.88"},
    {"name": "Роутер",     "host": "192.168.1.1"},
    {"name": "Cloudflare", "host": "1.1.1.1"},
    {"name": "Quad9",      "host": "9.9.9.9"},
]
PING_TIMEOUT_SEC  = 1.0

# Утечка
LEAK_MIN_DELTA_MB     = 100       # порог, с которого страница появляется
LEAK_CALM_WINDOW_SEC  = 30        # окно стабилизации
LEAK_CALM_THRESHOLD_MB= 10        # прирост < 10MB считается "спокойствием"
LEAK_TOP_N            = 5         # сколько процессов показывать

# FPS
RTSS_UPDATE_MS    = 500           # как часто читать RTSS

# Нотификации
NOTIFY_HISTORY    = 50            # сколько последних хранить

# Управление питанием
POWER_CONFIRM_SEC = 5             # задержка перед выполнением команды (на ESP)
POWER_CMD_SLEEP   = "rundll32.exe powrprof.dll,SetSuspendState 0,1,0"
POWER_CMD_REBOOT  = "shutdown /r /t 0"
POWER_CMD_OFF     = "shutdown /s /t 0"

# Часовой пояс
TZ_OFFSET_HOURS   = 3             # Москва UTC+3

# Отладка
DEBUG             = False         # если True — подробный вывод
LOG_TO_FILE       = True          # писать лог в файл
LOG_FILE          = "pc_monitor.log"
