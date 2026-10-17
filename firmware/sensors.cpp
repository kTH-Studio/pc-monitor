#include "sensors.h"
#include <Wire.h>
#include <Adafruit_BME280.h>
#include "display.h"

//  BME280

static Adafruit_BME280 bme;
static bool     bmeOk      = false;
static float    bmeTemp    = 0;
static float    bmeHum     = 0;
static float    bmePressH  = 0;

//  LDR

static bool     ldrOk      = true;      // LDR — всегда "ок", если пин читается
static uint8_t  ldrPct     = 0;

// Калибровка LDR (подстраивается под реальные условия)
// ADC ESP32 — 12 бит, 0..4095
// Делитель: VCC — LDR — GPIO34 — R(10k) — GND
// Ярко → сопротивление LDR мало → на пине много → значение высокое
// Темно → сопротивление LDR велико → на пине мало → значение низкое
#define LDR_ADC_MIN   100     // почти темно (значение АЦП)
#define LDR_ADC_MAX   4000    // ярко

//  Инициализация

void sensors_init() {
    // BME280
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    if (bme.begin(BME280_ADDR, &Wire)) {
        bmeOk = true;
        bme.setSampling(Adafruit_BME280::MODE_FORCED,
                        Adafruit_BME280::SAMPLING_X2,   // температура
                        Adafruit_BME280::SAMPLING_X16,  // давление
                        Adafruit_BME280::SAMPLING_X1,   // влажность
                        Adafruit_BME280::FILTER_X16,
                        Adafruit_BME280::STANDBY_MS_500);
        Serial.println("[SENSORS] BME280 OK @0x76");
    } else {
        bmeOk = false;
        Serial.println("[SENSORS] BME280 NOT FOUND!");
    }

    // LDR
    pinMode(PIN_LDR, INPUT);
    analogReadResolution(12);        // 12 бит
    analogSetAttenuation(ADC_11db);  // диапазон ~0..3.3V

    // Первое чтение
    sensors_update();

    // Отметки для boot-экрана
    extern void boot_check_bme(bool ok);
    extern void boot_check_ldr(bool ok);
    boot_check_bme(bmeOk);
    boot_check_ldr(ldrOk);
}

//  Опрос

void sensors_start_polling() {
    // Ничего не делаем — опрос идёт через sensors_update() из loop()
}

void sensors_update() {
    // BME280
    if (bmeOk) {
        bme.takeForcedMeasurement();
        bmeTemp   = bme.readTemperature();
        bmeHum    = bme.readHumidity();
        bmePressH = bme.readPressure() / 100.0f;  // Па → гПа

        // Проверка на NaN (иногда бывает при сбое)
        if (isnan(bmeTemp)   || bmeTemp   < -50 || bmeTemp > 100) bmeTemp   = 0;
        if (isnan(bmeHum)    || bmeHum    < 0   || bmeHum  > 100) bmeHum    = 0;
        if (isnan(bmePressH) || bmePressH < 300 || bmePressH > 1100) bmePressH = 0;
    }

    // LDR
    // Среднее по 8 замерам — сглаживание
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 8; i++) {
        sum += analogRead(PIN_LDR);
    }
    uint16_t raw = sum / 8;

    // Маппинг в 0..100%
    if (raw <= LDR_ADC_MIN)      ldrPct = 0;
    else if (raw >= LDR_ADC_MAX) ldrPct = 100;
    else {
        ldrPct = (uint32_t)(raw - LDR_ADC_MIN) * 100 / (LDR_ADC_MAX - LDR_ADC_MIN);
    }
}

//  Геттеры

bool  sensors_bme_ok()    { return bmeOk; }
float sensors_temp()      { return bmeTemp; }
float sensors_hum()       { return bmeHum; }
float sensors_press_hpa() { return bmePressH; }
float sensors_press_mm()  {
    // 1 гПа ≈ 0.750062 мм рт. ст.
    return bmePressH * 0.750062f;
}

uint8_t sensors_light_pct() { return ldrPct; }
bool    sensors_ldr_ok()    { return ldrOk; }
