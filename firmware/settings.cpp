#include "settings.h"
#include <Preferences.h>

//  Текущее состояние (кэш в RAM, чтобы не дёргать NVS)

Lang currentLang = LANG_RU;

static Preferences prefs;

// Дефолты
static const PingTarget DEF_PING[5] = {
    {"Google",     "8.8.8.8"},
    {"Яндекс",     "77.88.55.88"},
    {"Роутер",     "192.168.1.1"},
    {"Cloudflare", "1.1.1.1"},
    {"Quad9",      "9.9.9.9"}
};

static const uint8_t DEF_PAGE_ORDER[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };

// Кэш
static String   _wifiSsid, _wifiPass;
static bool     _hasWifi     = false;
static uint8_t  _brightness  = 100;
static uint8_t  _nightBright = 1;
static NightMode _nightMode  = NIGHT_OFF;
static uint8_t  _nightOn     = 23;
static uint8_t  _nightOff    = 7;

static uint16_t _notifyMs    = NOTIFY_DURATION_MS;
static uint16_t _mediaPause  = MEDIA_PAUSE_MS;
static uint16_t _powerMs     = POWER_CONFIRM_MS;

static Thresholds _thr = { 85, 90, 90, 90, 200, 500 };

static PingTarget _ping[5];

static uint8_t _pageOrder[8];
static uint8_t _pageCount = 8;

// Кнопки: [btn][type]
static ButtonAction _btns[NUM_BUTTONS][PRESS_COUNT];

static String _mediaPriority = "Spotify;YouTube;VLC;Browser";
static String _conditions[3] = { "auto", "auto", "auto" };

//  Дефолтные кнопки

static void load_default_buttons() {
    // BTN1: короткое — пред. страница
    _btns[0][PRESS_SHORT]  = ACT_PREV_PAGE;
    _btns[0][PRESS_LONG]   = ACT_NONE;
    _btns[0][PRESS_DOUBLE] = ACT_NONE;

    // BTN2: короткое — пауза/плей
    _btns[1][PRESS_SHORT]  = ACT_PLAY_PAUSE;
    _btns[1][PRESS_LONG]   = ACT_NONE;
    _btns[1][PRESS_DOUBLE] = ACT_NONE;

    // BTN3: короткое — след. трек
    _btns[2][PRESS_SHORT]  = ACT_NEXT_TRACK;
    _btns[2][PRESS_LONG]   = ACT_NONE;
    _btns[2][PRESS_DOUBLE] = ACT_NONE;

    // BTN4: короткое — след. страница
    _btns[3][PRESS_SHORT]  = ACT_NEXT_PAGE;
    _btns[3][PRESS_LONG]   = ACT_NONE;
    _btns[3][PRESS_DOUBLE] = ACT_POWER_CANCEL;

    // BTN5: короткое — ночной режим
    _btns[4][PRESS_SHORT]  = ACT_NIGHT_TOGGLE;
    _btns[4][PRESS_LONG]   = ACT_LANG_TOGGLE;
    _btns[4][PRESS_DOUBLE] = ACT_NONE;
}

//  Инициализация

void settings_init() {
    prefs.begin(NVS_NAMESPACE, false);

    // Язык
    currentLang = (Lang)prefs.getUChar("lang", LANG_RU);

    // WiFi
    _wifiSsid = prefs.getString("wifi_ssid", "");
    _wifiPass = prefs.getString("wifi_pass", "");
    _hasWifi  = (_wifiSsid.length() > 0);

    // Отображение
    _brightness  = prefs.getUChar("bright", 100);
    _nightBright = prefs.getUChar("nbright", 1);

    // Ночной
    _nightMode = (NightMode)prefs.getUChar("night_mode", NIGHT_OFF);
    _nightOn   = prefs.getUChar("night_on", 23);
    _nightOff  = prefs.getUChar("night_off", 7);

    // Тайминги
    _notifyMs   = prefs.getUShort("notify_ms", NOTIFY_DURATION_MS);
    _mediaPause = prefs.getUShort("media_ms", MEDIA_PAUSE_MS);
    _powerMs    = prefs.getUShort("power_ms", POWER_CONFIRM_MS);

    // Пороги
    _thr.gpuTemp = prefs.getUChar("thr_gpu", 85);
    _thr.cpuTemp = prefs.getUChar("thr_cpu", 90);
    _thr.ramPct  = prefs.getUChar("thr_ram", 90);
    _thr.diskPct = prefs.getUChar("thr_disk", 90);
    _thr.pingMs  = prefs.getUShort("thr_ping", 200);
    _thr.leakMB  = prefs.getUShort("thr_leak", 500);

    // Пинг-цели
    for (uint8_t i = 0; i < 5; i++) {
        char kName[16], kHost[16];
        snprintf(kName, sizeof(kName), "ping%d_n", i);
        snprintf(kHost, sizeof(kHost), "ping%d_h", i);
        String n = prefs.getString(kName, DEF_PING[i].name);
        String h = prefs.getString(kHost, DEF_PING[i].host);
        strncpy(_ping[i].name, n.c_str(), sizeof(_ping[i].name) - 1);
        strncpy(_ping[i].host, h.c_str(), sizeof(_ping[i].host) - 1);
    }

    // Порядок страниц
    size_t sz = prefs.getBytes("page_ord", _pageOrder, sizeof(_pageOrder));
    if (sz != sizeof(_pageOrder)) {
        memcpy(_pageOrder, DEF_PAGE_ORDER, sizeof(_pageOrder));
    }
    _pageCount = prefs.getUChar("page_cnt", 8);

    // Кнопки
    for (uint8_t b = 0; b < NUM_BUTTONS; b++) {
        for (uint8_t t = 0; t < PRESS_COUNT; t++) {
            char k[12];
            snprintf(k, sizeof(k), "btn%d_%d", b, t);
            _btns[b][t] = (ButtonAction)prefs.getUChar(k, 255);
        }
    }
    // Если хоть одна 255 — значит не задано, ставим дефолт
    if (_btns[0][PRESS_SHORT] == (ButtonAction)255) {
        load_default_buttons();
    }

    _mediaPriority = prefs.getString("media_prio", "Spotify;YouTube;VLC;Browser");

    _conditions[0] = prefs.getString("cond_leak",  "auto");
    _conditions[1] = prefs.getString("cond_disks", "auto");
    _conditions[2] = prefs.getString("cond_fps",   "auto");

    Serial.println("[SETTINGS] loaded");
}

//  Сохранение

void settings_save() {
    prefs.putUChar("lang", currentLang);
    prefs.putUChar("bright", _brightness);
    prefs.putUChar("nbright", _nightBright);
    prefs.putUChar("night_mode", _nightMode);
    prefs.putUChar("night_on", _nightOn);
    prefs.putUChar("night_off", _nightOff);

    prefs.putUShort("notify_ms", _notifyMs);
    prefs.putUShort("media_ms", _mediaPause);
    prefs.putUShort("power_ms", _powerMs);

    prefs.putUChar("thr_gpu", _thr.gpuTemp);
    prefs.putUChar("thr_cpu", _thr.cpuTemp);
    prefs.putUChar("thr_ram", _thr.ramPct);
    prefs.putUChar("thr_disk", _thr.diskPct);
    prefs.putUShort("thr_ping", _thr.pingMs);
    prefs.putUShort("thr_leak", _thr.leakMB);

    for (uint8_t i = 0; i < 5; i++) {
        char kName[16], kHost[16];
        snprintf(kName, sizeof(kName), "ping%d_n", i);
        snprintf(kHost, sizeof(kHost), "ping%d_h", i);
        prefs.putString(kName, _ping[i].name);
        prefs.putString(kHost, _ping[i].host);
    }

    prefs.putBytes("page_ord", _pageOrder, sizeof(_pageOrder));
    prefs.putUChar("page_cnt", _pageCount);

    for (uint8_t b = 0; b < NUM_BUTTONS; b++) {
        for (uint8_t t = 0; t < PRESS_COUNT; t++) {
            char k[12];
            snprintf(k, sizeof(k), "btn%d_%d", b, t);
            prefs.putUChar(k, _btns[b][t]);
        }
    }

    prefs.putString("media_prio", _mediaPriority);

    Serial.println("[SETTINGS] saved");
        prefs.putString("cond_leak",  _conditions[0]);
    prefs.putString("cond_disks", _conditions[1]);
    prefs.putString("cond_fps",   _conditions[2]);
}

//  Сброс

void settings_reset() {
    prefs.clear();
    settings_init();
    Serial.println("[SETTINGS] reset");
}

//  Геттеры / сеттеры

Lang settings_get_lang() { return currentLang; }
void settings_set_lang(Lang l) { currentLang = l; }

bool settings_has_wifi() { return _hasWifi; }

String settings_get_wifi_ssid() { return _wifiSsid; }
String settings_get_wifi_pass() { return _wifiPass; }

void settings_set_wifi(const String& ssid, const String& pass) {
    _wifiSsid = ssid;
    _wifiPass = pass;
    _hasWifi  = (ssid.length() > 0);
    prefs.putString("wifi_ssid", _wifiSsid);
    prefs.putString("wifi_pass", _wifiPass);
}

void settings_clear_wifi() {
    _wifiSsid = "";
    _wifiPass = "";
    _hasWifi  = false;
    prefs.remove("wifi_ssid");
    prefs.remove("wifi_pass");
}

uint8_t settings_get_brightness()        { return _brightness; }
void    settings_set_brightness(uint8_t v){ _brightness = v; prefs.putUChar("bright", v); }

uint8_t settings_get_night_brightness()        { return _nightBright; }
void    settings_set_night_brightness(uint8_t v){ _nightBright = v; prefs.putUChar("nbright", v); }

NightMode settings_get_night_mode()      { return _nightMode; }
void      settings_set_night_mode(NightMode m){ _nightMode = m; prefs.putUChar("night_mode", m); }

uint8_t settings_get_night_hour_on()  { return _nightOn; }
uint8_t settings_get_night_hour_off() { return _nightOff; }

void settings_set_night_time(uint8_t on_h, uint8_t off_h) {
    _nightOn  = on_h;
    _nightOff = off_h;
    prefs.putUChar("night_on", on_h);
    prefs.putUChar("night_off", off_h);
}

uint16_t settings_get_notify_duration() { return _notifyMs; }
void settings_set_notify_duration(uint16_t ms) { _notifyMs = ms; prefs.putUShort("notify_ms", ms); }

uint16_t settings_get_media_pause() { return _mediaPause; }
void settings_set_media_pause(uint16_t ms) { _mediaPause = ms; prefs.putUShort("media_ms", ms); }

uint16_t settings_get_power_confirm() { return _powerMs; }
void settings_set_power_confirm(uint16_t ms) { _powerMs = ms; prefs.putUShort("power_ms", ms); }

Thresholds settings_get_thresholds() { return _thr; }
void settings_set_thresholds(const Thresholds& t) { _thr = t; settings_save(); }

uint8_t settings_get_ping_count() { return 5; }
PingTarget settings_get_ping(uint8_t idx) {
    if (idx > 4) idx = 0;
    return _ping[idx];
}
void settings_set_ping(uint8_t idx, const char* name, const char* host) {
    if (idx > 4) return;
    strncpy(_ping[idx].name, name, sizeof(_ping[idx].name) - 1);
    strncpy(_ping[idx].host, host, sizeof(_ping[idx].host) - 1);
    settings_save();
}

uint8_t settings_get_page_order(uint8_t idx) {
    if (idx >= _pageCount) return 0;
    return _pageOrder[idx];
}
void settings_set_page_order(const uint8_t* order, uint8_t count) {
    if (count > 8) count = 8;
    memcpy(_pageOrder, order, count);
    _pageCount = count;
    prefs.putBytes("page_ord", _pageOrder, sizeof(_pageOrder));
    prefs.putUChar("page_cnt", _pageCount);
}
uint8_t settings_get_page_count() { return _pageCount; }

ButtonAction settings_get_button(uint8_t btn, PressType type) {
    if (btn >= NUM_BUTTONS || type >= PRESS_COUNT) return ACT_NONE;
    return _btns[btn][type];
}
void settings_set_button(uint8_t btn, PressType type, ButtonAction act) {
    if (btn >= NUM_BUTTONS || type >= PRESS_COUNT) return;
    _btns[btn][type] = act;
    char k[12];
    snprintf(k, sizeof(k), "btn%d_%d", btn, type);
    prefs.putUChar(k, act);
}

String settings_get_media_priority() { return _mediaPriority; }
void settings_set_media_priority(const String& list) {
    _mediaPriority = list;
    prefs.putString("media_prio", list);
}

const char* settings_get_condition(uint8_t idx) {
    if (idx > 2) return "auto";
    return _conditions[idx].c_str();
}

void settings_set_condition(uint8_t idx, const char* val) {
    if (idx > 2 || !val) return;
    _conditions[idx] = val;
    const char* keys[3] = { "cond_leak", "cond_disks", "cond_fps" };
    prefs.putString(keys[idx], _conditions[idx]);
}