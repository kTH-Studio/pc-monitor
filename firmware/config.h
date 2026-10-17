#pragma once

//  PC Monitor v.5.1.2 by KTH-Studio

//  ВЕРСИЯ 
#define FW_NAME      "PC Monitor"
#define FW_VERSION   "5.1.2"
#define FW_AUTHOR    "KTH-Studio"

//  ПИНЫ — КНОПКИ 
#define PIN_BTN1   4
#define PIN_BTN2   27
#define PIN_BTN3   14
#define PIN_BTN4   5
#define PIN_BTN5   23

//  ПИНЫ — LED 
#define PIN_LED_WHITE   13
#define PIN_LED_BLUE    16
#define PIN_LED_GREEN   17
#define PIN_LED_YELLOW  18
#define PIN_LED_RED     19

//  ПИНЫ — I2C 
// Аппаратный I2C (OLED32 + BME280)
#define PIN_I2C_SDA     21
#define PIN_I2C_SCL     22

// Софт-I2C (OLED64)
#define PIN_SOFT_SDA    25
#define PIN_SOFT_SCL    26

//  ПИНЫ — LDR 
#define PIN_LDR         34   // input only

//  I2C АДРЕСА 
#define OLED64_ADDR     0x3C
#define OLED32_ADDR     0x3C
#define BME280_ADDR     0x76   // или 0x77

//  РАЗМЕРЫ ЭКРАНОВ 
#define OLED64_W   128
#define OLED64_H   64
#define OLED32_W   128
#define OLED32_H   32

//  ИНДИКАТОР СТРАНИЦ 
#define PAGE_IND_H       2      // высота заливки активного сегмента
#define PAGE_IND_GAP     2      // зазор между сегментами
#define PAGE_IND_STEP_MS 500    // шаг заливки
#define PAGE_IND_PX      2      // пикселей за шаг

//  КОЛИЧЕСТВО 
#define NUM_LEDS     5
#define NUM_BUTTONS  5
#define NUM_PAGES    8          // базовых страниц (без условных)

//  ТАЙМИНГИ 
#define PAGE_SWITCH_MS      10000   // авто-смена страницы
#define BOOT_MIN_MS         3000    // минимум на экране инициализации
#define LOGO_DURATION_MS    3000    // показ логотипа
#define SENSOR_POLL_MS      1000    // опрос BME280
#define DATA_TIMEOUT_MS     3000    // таймаут данных с ПК

//  ОВЕРЛЕИ 
#define NOTIFY_DURATION_MS  5000    // показ нотификации
#define MEDIA_PAUSE_MS      10000   // медиа после паузы
#define POWER_CONFIRM_MS    5000    // отсчёт выключения
#define FPS_FRAME_PAD       2       // отступ рамки FPS

//  LED 
#define LED_STEP_PERCENT    20      // шаг на LED

//  НОЧНОЙ РЕЖИМ 
#define NIGHT_BRIGHTNESS    1       // яркость 1%

//  СЕТЬ 
#define WIFI_STA_TIMEOUT_MS 10000
#define AP_SSID             "PCMonitor"
#define AP_PASS             "12345678"
#define WS_PORT             81
#define HTTP_PORT           80

//  NVS 
#define NVS_NAMESPACE       "pcmon"

//  СВЯЗЬ С ПК 
#define SERIAL_BAUD         115200
#define COMMS_PORT          8888    // TCP-порт для данных с ПК
#define COMMS_BUF_SIZE      4096    // буфер JSON
