//  PC Monitor v.5.0 by KTH-Studio
#include "config.h"
#include "lang.h"
#include "display.h"
#include "input.h"
#include "sensors.h"
#include "pages.h"
#include "overlays.h"
#include "comms.h"
#include "webserver.h"
#include "settings.h"
#include "clock.h"

//  Состояния системы
 
enum SystemState : uint8_t {
    ST_BOOT_INIT,
    ST_BOOT_LOGO,
    ST_RUNNING,
    ST_NIGHT
};

static SystemState sysState = ST_BOOT_INIT;
static uint32_t bootStart    = 0;
static uint32_t lastPageSw   = 0;
static uint32_t lastSensor   = 0;
static uint32_t lastOverlay  = 0;

// Ночной режим
static bool     nightActive  = false;
static uint32_t nightEntered = 0;
 
//  Обработка событий кнопок
 
static void handle_button(const ButtonEvent& e) {
    if (!e.valid) return;

    ButtonAction act = settings_get_button(e.btn, e.type);
    if (act == ACT_NONE) return;

    Serial.printf("[BTN] BTN%d type=%d -> action=%d\n", e.btn + 1, e.type, act);

    switch (act) {
        case ACT_NEXT_PAGE:    pages_next();       lastPageSw = millis(); break;
        case ACT_PREV_PAGE:    pages_prev();       lastPageSw = millis(); break;
        case ACT_NEXT_TRACK:   comms_send_media("next");      break;
        case ACT_PREV_TRACK:   comms_send_media("prev");      break;
        case ACT_PLAY_PAUSE:   comms_send_media("playpause"); break;
        case ACT_NIGHT_TOGGLE: {
            nightActive = !nightActive;
            if (nightActive) {
                nightEntered = millis();
                display_set_night_brightness(settings_get_night_brightness());
                led_all_off();
            } else {
                display_set_brightness(settings_get_brightness());
            }
            break;
        }
        case ACT_LANG_TOGGLE: {
            Lang l = settings_get_lang();
            settings_set_lang(l == LANG_RU ? LANG_EN : LANG_RU);
            comms_send_lang(settings_get_lang());
            break;
        }
        case ACT_POWER_SLEEP:  power_request(PWR_SLEEP);  break;
        case ACT_POWER_REBOOT: power_request(PWR_REBOOT); break;
        case ACT_POWER_OFF:    power_request(PWR_OFF);    break;
        case ACT_POWER_CANCEL: power_cancel();            break;
        case ACT_ESP_REBOOT:
            Serial.println("[SYS] restart requested");
            delay(100);
            ESP.restart();
            break;
        case ACT_RESET_COUNTERS:
            // (сброс счётчиков сети/утечки — на стороне ПК)
            break;
        default: break;
    }
}

//  Ночной режим — отрисовка
 
static void render_night() {
    U8G2* big   = display_big();
    U8G2* small = display_small();

    int h, m, d, mon, y, wd;
    bool hasTime = clock_get(h, m, d, mon, y, wd);

    //  Большой: часы 
    big->clearBuffer();
    if (hasTime) {
        char buf[32];
        big->setFont(FONT_BIG_HUGE);
        snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
        int w = big->getStrWidth(buf);
        big->drawStr((OLED64_W - w) / 2, 38, buf);

        big->setFont(FONT_BIG_NORMAL);
        snprintf(buf, sizeof(buf), "%d %s %d", d, T_Month(mon - 1), y);
        w = big->getStrWidth(buf);
        big->drawStr((OLED64_W - w) / 2, 52, buf);

        big->setFont(FONT_BIG_SMALL);
        const char* day = T_Day(wd - 1);
        w = big->getStrWidth(day);
        big->drawStr((OLED64_W - w) / 2, 62, day);
    } else {
        big->setFont(FONT_BIG_NORMAL);
        big->drawStr(20, 35, "--:--");
    }
    big->sendBuffer();

    //  Малый: климат 
    small->clearBuffer();
    small->setFont(FONT_SMALL);
    char buf[40];
    snprintf(buf, sizeof(buf), "%.1f°C  %.0f%%", sensors_temp(), sensors_hum());
    small->drawStr(0, 10, buf);
    snprintf(buf, sizeof(buf), "%.0fmm", sensors_press_mm());
    small->drawStr(0, 24, buf);
    small->sendBuffer();
}
 
//  Проверка автовключения ночного режима (по времени)
 
static void check_night_auto() {
    if (settings_get_night_mode() != NIGHT_BY_TIME) return;
    if (!clock_synced()) return;

    int h, m, d, mon, y, wd;
    if (!clock_get(h, m, d, mon, y, wd)) return;

    uint8_t onH  = settings_get_night_hour_on();
    uint8_t offH = settings_get_night_hour_off();

    bool inNight;
    if (onH < offH) {
        inNight = (h >= onH && h < offH);
    } else {
        // переход через полночь
        inNight = (h >= onH || h < offH);
    }

    if (inNight && !nightActive) {
        nightActive = true;
        display_set_night_brightness(settings_get_night_brightness());
        led_all_off();
    } else if (!inNight && nightActive) {
        nightActive = false;
        display_set_brightness(settings_get_brightness());
    }
}
 
//  SETUP
 
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(100);
    Serial.println();
    Serial.println("==============================");
    Serial.printf("  %s v.%s\n", FW_NAME, FW_VERSION);
    Serial.printf("  by %s\n", FW_AUTHOR);
    Serial.println("==============================");

    // 1. Настройки
    settings_init();

    // 2. Железо
    input_init();
    display_init();
    sensors_init();

    // 3. Boot-экран
    display_boot_init_begin();
    bootStart = millis();
    sysState  = ST_BOOT_INIT;

    // 4. Связь + веб + часы
    comms_init();
    webserver_init();
    clock_init();

    // Установить яркость из настроек
    display_set_brightness(settings_get_brightness());

    Serial.println("[SYS] setup done");
}

//  LOOP
 
void loop() {
    uint32_t now = millis();

    //  Базовые апдейты (всегда) 
    input_update();
    comms_update();
    webserver_update();
    clock_update();

    //  Кнопки 
    ButtonEvent e = input_get_event();
    if (e.valid) handle_button(e);

    //  Датчики (раз в секунду) 
    if (now - lastSensor >= SENSOR_POLL_MS) {
        sensors_update();
        lastSensor = now;
    }

    //  Ночной авторежим 
    if (sysState == ST_RUNNING || sysState == ST_NIGHT) {
        check_night_auto();
    }

    //  Машина состояний
     
    switch (sysState) {

        //  BOOT: инициализация 
        case ST_BOOT_INIT: {
            display_boot_init_update();

            // Условие завершения: минимум 3 сек И все проверки done
            if (now - bootStart > BOOT_MIN_MS && display_boot_done()) {
                display_logo_show();
                sysState = ST_BOOT_LOGO;
                bootStart = now;
                Serial.println("[SYS] boot init done, showing logo");
            }
            break;
        }

        //  BOOT: логотип 
        case ST_BOOT_LOGO: {
            if (now - bootStart > LOGO_DURATION_MS) {
                sysState = ST_RUNNING;
                pages_init();
                lastPageSw = now;
                Serial.println("[SYS] running");
            }
            break;
        }

        //  RUNNING 
        case ST_RUNNING: {
            // Если ночной активен — переключаемся
            if (nightActive) {
                sysState = ST_NIGHT;
                nightEntered = now;
                break;
            }

            // Оверлеи (FPS, нотификации, медиа, питание)
            bool overlay = overlays_update();

            // Если оверлей активен — страницы не листаем и не рисуем
            if (overlay) {
                lastPageSw = now;  // сбрасываем таймер, чтобы страница не ушла во время оверлея
                break;
            }

            // Авто-смена страниц
            if (now - lastPageSw >= PAGE_SWITCH_MS) {
                pages_next();
                lastPageSw = now;
            }

            // Рендер страниц
            pages_render();
            break;
        }

        //  NIGHT 
        case ST_NIGHT: {
            // Оверлеи работают и в ночном (FPS, медиа, питание)
            // Нонотификации — приглушены (упрощение: показываем как обычно)
            bool overlay = overlays_update();
            if (overlay) break;

            render_night();

            // Выход из ночного
            if (!nightActive) {
                sysState = ST_RUNNING;
                display_set_brightness(settings_get_brightness());
                lastPageSw = now;
                Serial.println("[SYS] night off");
            }
            break;
        }
    }

    // Небольшая пауза — чтобы не крутить loop на 100%
    delay(5);
}
