#include "overlays.h"
#include "display.h"
#include "input.h"
#include "settings.h"
#include "pages.h"
#include "lang.h"
#include <string.h>

//  Глобальные данные

MediaData g_media = {};
FpsData   g_fps   = {};

//  Внутреннее состояние

//  Медиа 
static uint32_t mediaHideAt = 0;    // когда скрыть (после паузы)
static uint32_t mediaMarquee = 0;

//  Нотификации 
#define NOTIFY_QUEUE_SIZE 8
static NotifyData notifyQueue[NOTIFY_QUEUE_SIZE];
static uint8_t    notifyHead = 0;
static uint8_t    notifyTail = 0;
static uint8_t    notifyCount = 0;
static uint32_t   notifyStart = 0;
static bool       notifyActive = false;

//  FPS 
// просто g_fps.present

//  Питание 
static PowerAction pwrAction = PWR_NONE;
static uint32_t    pwrStart = 0;
static uint32_t    pwrDuration = 0;

//  Инициализация

void overlays_init() {
    memset(&g_media, 0, sizeof(g_media));
    memset(&g_fps, 0, sizeof(g_fps));
    notifyHead = notifyTail = notifyCount = 0;
    notifyActive = false;
    pwrAction = PWR_NONE;
}

//  МЕДИА

void media_set(const MediaData& m) {
    bool wasPlaying = g_media.playing;
    g_media = m;

    // Если данные обновились — сбрасываем таймер скрытия (трек сменился)
    if (m.updated) {
        mediaHideAt = 0;
    }

    // Если играет — сбрасываем таймер
    if (g_media.playing) {
        mediaHideAt = 0;
    }
    // Если пауза и раньше играло — стартуем таймер
    else if (wasPlaying && !g_media.playing) {
        mediaHideAt = millis() + settings_get_media_pause();
    }
    // Если не играет и не играло (только пришло) — ставим таймер сразу
    else if (!g_media.playing && mediaHideAt == 0) {
        mediaHideAt = millis() + settings_get_media_pause();
    }
}

static bool media_should_show() {
    if (!g_media.present) return false;
    if (g_media.playing)  return true;
    if (mediaHideAt > 0 && millis() < mediaHideAt) return true;
    return false;
}

static void render_media(U8G2* u) {
    u->setDrawColor(1);
    u->drawBox(0, 0, OLED32_W, OLED32_H);   // белый фон
    u->setDrawColor(0);                     // чёрный текст

    u->setFont(FONT_SMALL);

    // Строка 1: статус + исполнитель
    char buf[80];
    snprintf(buf, sizeof(buf), "%s %s",
             g_media.playing ? "\x95" : "\x96",   // ▶ / ⏸ (замени на свои)
             g_media.artist);
    u->drawStr(0, 7, buf);

    // Строка 2: трек
    u->drawStr(0, 15, g_media.track);

    // Строка 3: альбом
    u->drawStr(0, 23, g_media.album);

    // Строка 4: позиция + прогресс-бар 5px + длительность
    uint16_t pos = g_media.posSec;
    uint16_t dur = g_media.durSec;

    snprintf(buf, sizeof(buf), "%d:%02d", pos / 60, pos % 60);
    u->drawStr(0, 31, buf);

    // Прогресс-бар
    int barX = 30;
    int barW = 68;
    int barY = 26;
    int barH = 5;
    u->drawFrame(barX, barY, barW, barH);
    if (dur > 0) {
        int fill = (uint32_t)pos * (barW - 2) / dur;
        if (fill > barW - 2) fill = barW - 2;
        if (fill > 0) u->drawBox(barX + 1, barY + 1, fill, barH - 2);
    }

    snprintf(buf, sizeof(buf), "%d:%02d", dur / 60, dur % 60);
    u->drawStr(OLED32_W - 25, 31, buf);

    u->setDrawColor(1);   // вернуть нормальный цвет
}

//  НОТИФИКАЦИИ

void notify_push(const NotifyData& n) {
    // Если очередь полна — дропаем самую старую
    if (notifyCount >= NOTIFY_QUEUE_SIZE) {
        notifyTail = (notifyTail + 1) % NOTIFY_QUEUE_SIZE;
        notifyCount--;
    }
    notifyQueue[notifyHead] = n;
    notifyHead = (notifyHead + 1) % NOTIFY_QUEUE_SIZE;
    notifyCount++;

    // Вспышка белым
    led_flash_white(200);

    // Если FPS активен — копим, не показываем
    // Иначе — сразу показываем если ничего не показано
    if (!notifyActive && !g_fps.present && !power_active()) {
        notifyActive = true;
        notifyStart  = millis();
    }
}

static bool notify_should_show() {
    if (notifyCount == 0) return false;
    if (g_fps.present) return false;      // FPS главнее
    if (power_active()) return false;     // питание главнее
    return true;
}

static NotifyData& notify_current() {
    return notifyQueue[notifyTail];
}

static void notify_drop_current() {
    if (notifyCount == 0) return;
    notifyTail = (notifyTail + 1) % NOTIFY_QUEUE_SIZE;
    notifyCount--;
}

static void render_notify(U8G2* u) {
    NotifyData& n = notify_current();

    // Инверсия
    u->setDrawColor(1);
    u->drawBox(0, 0, OLED64_W, OLED64_H);
    u->setDrawColor(0);

    u->setFont(FONT_BIG_SMALL);

    // Строка 1: App +N слева, время справа
    char buf[80];
    if (n.count > 1) {
        snprintf(buf, sizeof(buf), "%s +%d", n.app, n.count);
    } else {
        snprintf(buf, sizeof(buf), "%s", n.app);
    }
    u->drawStr(0, 7, buf);

    int tw = u->getStrWidth(n.time);
    u->drawStr(OLED64_W - tw, 7, n.time);

    // Строка 2: тема (бегущая если длинная)
    // (для простоты — обрезка "...")
    // ... но по спецификации — бегущая. Сделаем через marquee.
    // Здесь пока просто обрезка:
    u->drawStr(0, 17, n.title);

    // Строки 3-6: тело (4 строки по 10px)
    // Разбиваем body на строки по ширине
    // Упрощённо — просто рисуем body как есть, ESP сам обрежет
    // (в реальности нужна разбивка)
    u->setFont(FONT_BIG_SMALL);
    const int lineH = 11;
    const int bodyX = 0;
    int bodyY = 27;
    int maxY = OLED64_H - 3;

    // Простой перенос посимвольно (без учёта слов)
    const char* p = n.body;
    char line[24];
    int lineLen = 0;
    while (*p && bodyY <= maxY) {
        // Оцениваем ширину
        char test[24];
        int tl = 0;
        while (*p && tl < 22) {
            test[tl++] = *p++;
        }
        test[tl] = 0;
        int w = u->getStrWidth(test);
        // Откатываем, если не влезает
        while (w > OLED64_W - 4 && tl > 0) {
            tl--;
            p--;
            test[tl] = 0;
            w = u->getStrWidth(test);
        }
        // Многоточие, если упёрлись в конец тела
        if (*p == 0 && p - n.body > 0 && bodyY + lineH > maxY) {
            // Последняя строка — добавить ...
        }
        u->drawStr(bodyX, bodyY, test);
        bodyY += lineH;
    }

    u->setDrawColor(1);   // вернуть нормальный цвет
}

//  FPS

void fps_set(const FpsData& f) {
    bool wasPresent = g_fps.present;
    g_fps = f;

    // Если игра завершилась — показать накопленные нотификации (последние 3)
    if (wasPresent && !g_fps.present) {
        // Оставляем только последние 3 в очереди
        while (notifyCount > 3) {
            notifyTail = (notifyTail + 1) % NOTIFY_QUEUE_SIZE;
            notifyCount--;
        }
        if (notifyCount > 0) {
            notifyActive = true;
            notifyStart  = millis();
        }
    }

    // Если игра запустилась — скрыть текущую нотификацию
    if (!wasPresent && g_fps.present) {
        notifyActive = false;
    }
}

static void render_fps(U8G2* u) {
    u->setFont(FONT_BIG_NORMAL);

    // Рамка по краю
    u->drawFrame(0, 0, OLED64_W, OLED64_H);

    // Контент по центру
    char buf[48];
    int cx = 4;

    // Строка 1: FPS + имя игры (бегущая если длинное)
    snprintf(buf, sizeof(buf), "FPS  %s", g_fps.game);
    u->setFont(FONT_BIG_NORMAL);
    u->drawStr(cx, 12, buf);

    // Строка 2: текущий FPS (крупно)
    u->setFont(FONT_BIG_LARGE);
    snprintf(buf, sizeof(buf), "%d", g_fps.current);
    u->drawStr(cx, 32, buf);

    // Строка 3: min/max + drop
    u->setFont(FONT_BIG_NORMAL);
    snprintf(buf, sizeof(buf), "%d/%dfps  %d%%",
             g_fps.minFps, g_fps.maxFps, g_fps.dropPct);
    u->drawStr(cx, 44, buf);

    // Строка 4: GPU/CPU load
    snprintf(buf, sizeof(buf), "GPU %d%%  CPU %d%%", g_fps.gpuLoad, g_fps.cpuLoad);
    u->drawStr(cx, 56, buf);
}

//  ПОДТВЕРЖДЕНИЕ ПИТАНИЯ

void power_request(PowerAction act) {
    if (act == PWR_NONE) return;
    pwrAction   = act;
    pwrStart    = millis();
    pwrDuration = settings_get_power_confirm();

    // Мигание красным
    led_blink_red(pwrDuration);
}

void power_cancel() {
    pwrAction = PWR_NONE;
    led_all_off();
}

bool power_active()      { return pwrAction != PWR_NONE; }
PowerAction power_current() { return pwrAction; }

static bool power_should_show() {
    return pwrAction != PWR_NONE;
}

static void render_power(U8G2* u) {
    u->setFont(FONT_SMALL);

    // Заголовок
    const char* title = "";
    switch (pwrAction) {
        case PWR_SLEEP:  title = T(S_POWER_SLEEP);  break;
        case PWR_REBOOT: title = T(S_POWER_REBOOT); break;
        case PWR_OFF:    title = T(S_POWER_OFF);    break;
        default: break;
    }
    u->drawStr(0, 8, title);

    // Отсчёт
    uint32_t elapsed = millis() - pwrStart;
    int left = (pwrDuration > elapsed) ? (pwrDuration - elapsed) / 1000 : 0;
    char buf[32];
    snprintf(buf, sizeof(buf), "%s %d...", T(S_POWER_COUNTDOWN), left);
    u->drawStr(0, 18, buf);

    // Подсказка отмены
    extern ButtonAction settings_get_button(uint8_t btn, PressType type);
    // Ищем, какая кнопка назначена на ACT_POWER_CANCEL
    uint8_t cancelBtn = 0;
    bool found = false;
    for (uint8_t b = 0; b < NUM_BUTTONS && !found; b++) {
        for (uint8_t t = 0; t < PRESS_COUNT; t++) {
            if (settings_get_button(b, t) == ACT_POWER_CANCEL) {
                cancelBtn = b + 1;
                found = true;
                break;
            }
        }
    }
    if (found) {
        snprintf(buf, sizeof(buf), "%s: BTN%d", T(S_CANCEL), cancelBtn);
        u->drawStr(0, 30, buf);
    }
}

//  ГЛАВНЫЙ АПДЕЙТ

bool overlays_update() {
    uint32_t now = millis();

    //  Питание (высший приоритет) 
    if (power_should_show()) {
        if (now - pwrStart >= pwrDuration) {
            // Время вышло — отправляем команду на ПК
            extern void comms_send_power(PowerAction act);
            comms_send_power(pwrAction);
            pwrAction = PWR_NONE;
            led_all_off();
        } else {
            U8G2* small = display_small();
            small->clearBuffer();
            render_power(small);
            small->sendBuffer();
            return true;
        }
    }

    //  FPS (большой, главный) 
    if (g_fps.present) {
        led_all_off();                    // LED выключены
        U8G2* big = display_big();
        big->clearBuffer();
        render_fps(big);
        big->sendBuffer();
        return true;
    }

    //  Нотификации (большой) 
    if (notify_should_show()) {
        if (!notifyActive) {
            notifyActive = true;
            notifyStart  = now;
        }

        uint16_t dur = settings_get_notify_duration();
        if (now - notifyStart >= dur) {
            // Время вышло — убираем текущую, берём следующую
            notify_drop_current();
            if (notifyCount > 0) {
                notifyStart = now;
            } else {
                notifyActive = false;
            }
        } else {
            U8G2* big = display_big();
            big->clearBuffer();
            render_notify(big);
            big->sendBuffer();
            return true;
        }
    } else {
        notifyActive = false;
    }

    //  Медиа (малый) 
    if (media_should_show()) {
        U8G2* small = display_small();
        small->clearBuffer();
        render_media(small);
        small->sendBuffer();
        return true;    // малый занят, страница не должна рисоваться
    }

    return false;   // оверлеев нет — рисуем страницы как обычно
}

//  ТИК ДЛЯ АНИМАЦИЙ (вызывать из loop часто)
