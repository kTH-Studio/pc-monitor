#include "webserver.h"
#include "settings.h"
#include "display.h"
#include "input.h"
#include "pages.h"
#include "overlays.h"
#include "comms.h"
#include "sensors.h"
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

//  Объекты
static AsyncWebServer httpServer(HTTP_PORT);
static AsyncWebSocket ws("/ws");
static DNSServer       dnsServer;

static bool     apMode = false;
static String   staIp  = "";
static uint32_t bootMs = 0;

//  Логи (кольцевой буфер)

// стало
#define LOG_BUF_SIZE 100
#define LOG_LINE_MAX 96

static char logBuf[LOG_BUF_SIZE][LOG_LINE_MAX];
static uint8_t logHead = 0;
static uint8_t logCount = 0;

void webserver_push_log(const char* line) {
    // Формат: "12345: текст"
    uint32_t ms = millis();
    snprintf(logBuf[logHead], LOG_LINE_MAX, "%lu: %s",
             (unsigned long)ms, line ? line : "");
    logHead = (logHead + 1) % LOG_BUF_SIZE;
    if (logCount < LOG_BUF_SIZE) logCount++;

    // WS
    JsonDocument doc;
    doc["type"] = "log";
    doc["line"] = logBuf[logHead == 0 ? LOG_BUF_SIZE - 1 : logHead - 1];
    String out;
    serializeJson(doc, out);
    ws.textAll(out);
}

//  WiFi — STA или AP

static bool try_sta() {
    if (!settings_has_wifi()) return false;

    String ssid = settings_get_wifi_ssid();
    String pass = settings_get_wifi_pass();

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    Serial.printf("[WIFI] connecting to %s", ssid.c_str());
    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < WIFI_STA_TIMEOUT_MS) {
        delay(200);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        staIp = WiFi.localIP().toString();
        Serial.printf("[WIFI] STA OK, IP %s\n", staIp.c_str());
        return true;
    }

    Serial.println("[WIFI] STA failed");
    return false;
}

static void start_ap() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    apMode = true;
    staIp = WiFi.softAPIP().toString();
    Serial.printf("[WIFI] AP mode, SSID %s, IP %s\n", AP_SSID, staIp.c_str());

    // DNS-перехватчик (всё → 192.168.4.1)
    dnsServer.start(53, "*", WiFi.softAPIP());
}

//  Обработчики API

//  /api/status 
static void api_status(AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["fw"]      = FW_VERSION;
    doc["name"]    = FW_NAME;
    doc["author"]  = FW_AUTHOR;
    doc["uptime"]  = millis() / 1000;
    doc["heap"]    = ESP.getFreeHeap();
    doc["rssi"]    = WiFi.RSSI();
    doc["ip"]      = staIp;
    doc["ap"]      = apMode;
    doc["ssid"]    = apMode ? String(AP_SSID) : WiFi.SSID();
    doc["lang"]    = (int)settings_get_lang();
    doc["linked"]  = comms_is_linked();
    doc["channel"] = (int)comms_channel();
    doc["page"]    = pages_current();
    doc["pages"]   = pages_total();
    doc["night"]   = (int)settings_get_night_mode();
        doc["flash_size"] = ESP.getFlashChipSize();
    doc["cpu_freq"]   = ESP.getCpuFreqMHz();

    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

//  /api/data (все датчики) 
static void api_data(AsyncWebServerRequest* req) {
    JsonDocument doc;

    // GPU
    {
        JsonObject g = doc["gpu"].to<JsonObject>();
        g["present"] = g_gpu.present;
        g["name"]    = g_gpu.name;
        g["load"]    = g_gpu.load;
        g["temp"]    = g_gpu.temp;
        g["vram_used"]  = g_gpu.vramUsed;
        g["vram_total"] = g_gpu.vramTotal;
        g["power"]   = g_gpu.power;
        g["fan"]     = g_gpu.fan;
        g["driver"]  = g_gpu.driver;
    }
    // CPU
    {
        JsonObject c = doc["cpu"].to<JsonObject>();
        c["present"] = g_cpu.present;
        c["name"]    = g_cpu.name;
        c["load"]    = g_cpu.load;
        c["temp"]    = g_cpu.temp;
        c["freq"]    = g_cpu.freq;
        c["power"]   = g_cpu.power;
        c["core_max"]= g_cpu.coreMax;
        c["fan"]     = g_cpu.fan;
        c["vcore"]   = g_cpu.vcore;
    }
    // RAM
    {
        JsonObject r = doc["ram"].to<JsonObject>();
        r["present"] = g_ram.present;
        r["name"]    = g_ram.name;
        r["load"]    = g_ram.load;
        r["used"]    = g_ram.used;
        r["total"]   = g_ram.total;
        r["freq"]    = g_ram.freq;
        r["type"]    = g_ram.type;
        r["temp"]    = g_ram.temp;
        r["voltage"] = g_ram.voltage;
        r["dual"]    = g_ram.dualChannel;
    }
    // Disks
    {
        JsonArray arr = doc["disks"].to<JsonArray>();
        for (uint8_t i = 0; i < g_diskCount; i++) {
            JsonObject d = arr.add<JsonObject>();
            d["letter"] = String(g_disks[i].letter);
            d["name"]   = g_disks[i].name;
            d["load"]   = g_disks[i].load;
            d["temp"]   = g_disks[i].temp;
            d["used"]   = g_disks[i].used;
            d["total"]  = g_disks[i].total;
            d["max_speed"]  = g_disks[i].maxSpeed;
            d["type"]       = g_disks[i].type;
            d["read_rate"]  = g_disks[i].readRate;
            d["write_rate"] = g_disks[i].writeRate;
            d["life_left"]  = g_disks[i].lifeLeft;
            d["activity"]   = g_disks[i].activity;
        }
    }
    // Net
    {
        JsonObject n = doc["net"].to<JsonObject>();
        n["present"]   = g_net.present;
        n["adapter"]   = g_net.adapter;
        n["down_mbit"] = g_net.downMbit;
        n["up_mbit"]   = g_net.upMbit;
        n["total_down_gb"] = g_net.totalDownGB;
        n["total_up_gb"]   = g_net.totalUpGB;
        n["ip"]        = g_net.ip;
        n["link_speed"]= g_net.linkSpeed;
    }
    // Ping
    {
        JsonObject p = doc["ping"].to<JsonObject>();
        p["present"] = g_ping.present;
        p["avg"]     = g_ping.avgMs;
        JsonArray arr = p["targets"].to<JsonArray>();
        for (uint8_t i = 0; i < g_ping.count; i++) {
            JsonObject t = arr.add<JsonObject>();
            t["name"] = g_ping.targets[i].name;
            t["ms"]   = g_ping.targets[i].ms;
        }
    }
    // Leak
    {
        JsonObject l = doc["leak"].to<JsonObject>();
        l["present"] = g_leak.present;
        l["max_delta_mb"] = g_leak.maxDeltaMB;
        JsonArray arr = l["items"].to<JsonArray>();
        for (uint8_t i = 0; i < g_leak.count; i++) {
            JsonObject it = arr.add<JsonObject>();
            it["name"]     = g_leak.items[i].name;
            it["delta_mb"] = g_leak.items[i].deltaMB;
        }
    }
    // FPS
    {
        JsonObject f = doc["fps"].to<JsonObject>();
        f["present"] = g_fps.present;
        f["game"]    = g_fps.game;
        f["current"] = g_fps.current;
        f["min"]     = g_fps.minFps;
        f["max"]     = g_fps.maxFps;
        f["drop_pct"]= g_fps.dropPct;
        f["gpu_load"]= g_fps.gpuLoad;
        f["cpu_load"]= g_fps.cpuLoad;
    }
    // Media
    {
        JsonObject m = doc["media"].to<JsonObject>();
        m["present"] = g_media.present;
        m["playing"] = g_media.playing;
        m["app"]     = g_media.app;
        m["artist"]  = g_media.artist;
        m["track"]   = g_media.track;
        m["album"]   = g_media.album;
        m["pos"]     = g_media.posSec;
        m["dur"]     = g_media.durSec;
    }
    // Climate
    {
        JsonObject cl = doc["climate"].to<JsonObject>();
        cl["temp"]     = sensors_temp();
        cl["hum"]      = sensors_hum();
        cl["press_mm"] = sensors_press_mm();
        cl["light"]    = sensors_light_pct();
    }

    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

//  /api/data (POST от ПК) 
static void api_data_post(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                          size_t index, size_t total) {
    if (index == 0) {
        // первый чанк — обрабатываем целиком (если помещается)
        comms_handle_http_body(data, len);
    }
}

//  /api/settings (GET) 
static void api_settings_get(AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["lang"]         = (int)settings_get_lang();
    doc["brightness"]   = settings_get_brightness();
    doc["night_bright"] = settings_get_night_brightness();
    doc["night_mode"]   = (int)settings_get_night_mode();
    doc["night_on"]     = settings_get_night_hour_on();
    doc["night_off"]    = settings_get_night_hour_off();
    doc["notify_ms"]    = settings_get_notify_duration();
    doc["media_ms"]     = settings_get_media_pause();
    doc["power_ms"]     = settings_get_power_confirm();
    doc["media_prio"]   = settings_get_media_priority();

    Thresholds t = settings_get_thresholds();
    JsonObject thr = doc["thresholds"].to<JsonObject>();
    thr["gpu_temp"]  = t.gpuTemp;
    thr["cpu_temp"]  = t.cpuTemp;
    thr["ram_pct"]   = t.ramPct;
    thr["disk_pct"]  = t.diskPct;
    thr["ping_ms"]   = t.pingMs;
    thr["leak_mb"]   = t.leakMB;

    // Пинг-цели
    JsonArray ping = doc["ping"].to<JsonArray>();
    for (uint8_t i = 0; i < 5; i++) {
        PingTarget p = settings_get_ping(i);
        JsonObject o = ping.add<JsonObject>();
        o["name"] = p.name;
        o["host"] = p.host;
    }

    // WiFi (без пароля)
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"]   = settings_get_wifi_ssid();
    wifi["has"]    = settings_has_wifi();
    wifi["ap"]     = apMode;
    wifi["ap_ssid"]= AP_SSID;

        JsonObject cond = doc["conditions"].to<JsonObject>();
    cond["leak"]  = settings_get_condition(0);
    cond["disks"] = settings_get_condition(1);
    cond["fps"]   = settings_get_condition(2);

    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

//  /api/settings (POST) 
static void api_settings_post(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                              size_t index, size_t total) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) {
        req->send(400, "text/plain", "bad json");
        return;
    }

    if (doc["lang"].is<int>())         settings_set_lang((Lang)doc["lang"].as<int>());
    if (doc["brightness"].is<int>())   settings_set_brightness(doc["brightness"]);
    if (doc["night_bright"].is<int>()) settings_set_night_brightness(doc["night_bright"]);
    if (doc["night_mode"].is<int>())   settings_set_night_mode((NightMode)doc["night_mode"].as<int>());
    if (doc["night_on"].is<int>() && doc["night_off"].is<int>())
        settings_set_night_time(doc["night_on"], doc["night_off"]);
    if (doc["notify_ms"].is<int>())    settings_set_notify_duration(doc["notify_ms"]);
    if (doc["media_ms"].is<int>())     settings_set_media_pause(doc["media_ms"]);
    if (doc["power_ms"].is<int>())     settings_set_power_confirm(doc["power_ms"]);
    if (doc["media_prio"].is<const char*>()) settings_set_media_priority(doc["media_prio"].as<String>());

    if (doc["thresholds"].is<JsonObject>()) {
        JsonObject t = doc["thresholds"];
        Thresholds thr = settings_get_thresholds();
        thr.gpuTemp  = t["gpu_temp"]  | thr.gpuTemp;
        thr.cpuTemp  = t["cpu_temp"]  | thr.cpuTemp;
        thr.ramPct   = t["ram_pct"]   | thr.ramPct;
        thr.diskPct  = t["disk_pct"]  | thr.diskPct;
        thr.pingMs   = t["ping_ms"]   | thr.pingMs;
        thr.leakMB   = t["leak_mb"]   | thr.leakMB;
        settings_set_thresholds(thr);
    }

    if (doc["ping"].is<JsonArray>()) {
        uint8_t i = 0;
        for (JsonObject o : doc["ping"].as<JsonArray>()) {
            if (i >= 5) break;
            settings_set_ping(i, o["name"] | "", o["host"] | "");
            i++;
        }
    }

    if (doc["wifi"].is<JsonObject>()) {
        JsonObject w = doc["wifi"];
        if (w["ssid"].is<const char*>() && w["pass"].is<const char*>()) {
            settings_set_wifi(w["ssid"].as<String>(), w["pass"].as<String>());
        }
        if (w["clear"] | false) {
            settings_clear_wifi();
        }
    }

    if (doc["conditions"].is<JsonObject>()) {
        JsonObject c = doc["conditions"];
        if (c["leak"].is<const char*>())  settings_set_condition(0, c["leak"]);
        if (c["disks"].is<const char*>()) settings_set_condition(1, c["disks"]);
        if (c["fps"].is<const char*>())   settings_set_condition(2, c["fps"]);
    }

    req->send(200, "application/json", "{\"ok\":true}");
}

//  /api/control (POST) — действия 
static void api_control_post(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                             size_t index, size_t total) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) {
        req->send(400, "text/plain", "bad json");
        return;
    }

    const char* action = doc["action"] | "";

    // --- Страницы ---
    if (!strcmp(action, "page_next"))       pages_next();
    else if (!strcmp(action, "page_prev"))  pages_prev();
    else if (!strcmp(action, "page_set")) {
        uint8_t idx = doc["value"] | 0;
        extern void pages_goto(uint8_t idx);
        pages_goto(idx);
    }
    // --- Ночной режим ---
    else if (!strcmp(action, "night_toggle")) {
        NightMode m = settings_get_night_mode();
        settings_set_night_mode(m == NIGHT_OFF ? NIGHT_MANUAL : NIGHT_OFF);
    }
    // --- Язык ---
    else if (!strcmp(action, "lang_toggle")) {
        Lang l = settings_get_lang();
        settings_set_lang(l == LANG_RU ? LANG_EN : LANG_RU);
        extern void comms_send_lang(Lang l);
        comms_send_lang(settings_get_lang());
    }
    // --- Яркость ---
    else if (!strcmp(action, "brightness")) {
        uint8_t v = doc["value"] | 100;
        extern void display_set_brightness(uint8_t);
        display_set_brightness(v);
        // Не сохраняем в NVS — только мгновенное применение
    }
    // --- ESP ---
    else if (!strcmp(action, "esp_restart")) {
        req->send(200, "application/json", "{\"ok\":true}");
        delay(500);
        ESP.restart();
        return;
    }
    else if (!strcmp(action, "esp_reset")) {
        settings_reset();
    }
    else if (!strcmp(action, "wifi_clear")) {
        settings_clear_wifi();
        req->send(200, "application/json", "{\"ok\":true}");
        delay(500);
        ESP.restart();
        return;
    }
    else if (!strcmp(action, "reset_counters")) {
        extern void comms_send_reset(const char* what);
        comms_send_reset("counters");
    }
    // --- Питание ПК ---
    else if (!strcmp(action, "power_sleep"))   power_request(PWR_SLEEP);
    else if (!strcmp(action, "power_reboot"))  power_request(PWR_REBOOT);
    else if (!strcmp(action, "power_off"))     power_request(PWR_OFF);
    else if (!strcmp(action, "power_cancel"))  power_cancel();
    // --- Медиа ---
    else if (!strcmp(action, "media_next"))      comms_send_media("next");
    else if (!strcmp(action, "media_prev"))      comms_send_media("prev");
    else if (!strcmp(action, "media_playpause")) comms_send_media("playpause");
    // --- Сбросы счётчиков (на ПК) ---
    else if (!strcmp(action, "reset_net")) {
        extern void comms_send_reset(const char* what);
        comms_send_reset("net");
    }
    else if (!strcmp(action, "reset_leak")) {
        extern void comms_send_reset(const char* what);
        comms_send_reset("leak");
    }
    else if (!strcmp(action, "reset_fps")) {
        extern void comms_send_reset(const char* what);
        comms_send_reset("fps");
    }
    else if (!strcmp(action, "reset_all")) {
        extern void comms_send_reset(const char* what);
        comms_send_reset("all");
    }

    req->send(200, "application/json", "{\"ok\":true}");
}

//  /api/pages (GET/POST) 
static void api_pages_get(AsyncWebServerRequest* req) {
    JsonDocument doc;
    JsonArray arr = doc["order"].to<JsonArray>();
    uint8_t count = settings_get_page_count();
    for (uint8_t i = 0; i < count; i++) {
        arr.add(settings_get_page_order(i));
    }
    doc["count"] = count;
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

static void api_pages_post(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                           size_t index, size_t total) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) {
        req->send(400, "text/plain", "bad json");
        return;
    }

    if (doc["order"].is<JsonArray>()) {
        uint8_t order[8];
        uint8_t count = 0;
        for (JsonVariant v : doc["order"].as<JsonArray>()) {
            if (count >= 8) break;
            order[count++] = v.as<uint8_t>();
        }
        settings_set_page_order(order, count);
    }

    req->send(200, "application/json", "{\"ok\":true}");
}

//  /api/buttons (GET/POST) 
static void api_buttons_get(AsyncWebServerRequest* req) {
    JsonDocument doc;
    JsonArray arr = doc["buttons"].to<JsonArray>();
    for (uint8_t b = 0; b < NUM_BUTTONS; b++) {
        JsonObject o = arr.add<JsonObject>();
        o["short"]  = (int)settings_get_button(b, PRESS_SHORT);
        o["long"]   = (int)settings_get_button(b, PRESS_LONG);
        o["double"] = (int)settings_get_button(b, PRESS_DOUBLE);
    }
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

static void api_buttons_post(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                             size_t index, size_t total) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) {
        req->send(400, "text/plain", "bad json");
        return;
    }

    if (doc["buttons"].is<JsonArray>()) {
        uint8_t b = 0;
        for (JsonObject o : doc["buttons"].as<JsonArray>()) {
            if (b >= NUM_BUTTONS) break;
            if (o["short"].is<int>())  settings_set_button(b, PRESS_SHORT,  (ButtonAction)o["short"].as<int>());
            if (o["long"].is<int>())   settings_set_button(b, PRESS_LONG,   (ButtonAction)o["long"].as<int>());
            if (o["double"].is<int>()) settings_set_button(b, PRESS_DOUBLE, (ButtonAction)o["double"].as<int>());
            b++;
        }
    }

    req->send(200, "application/json", "{\"ok\":true}");
}

//  /api/logs (GET) 
static void api_logs_get(AsyncWebServerRequest* req) {
    String out = "[\n";
    uint8_t start = (logHead + LOG_BUF_SIZE - logCount) % LOG_BUF_SIZE;
    for (uint8_t i = 0; i < logCount; i++) {
        uint8_t idx = (start + i) % LOG_BUF_SIZE;
        out += "\"";
        // Экранируем кавычки
        for (const char* p = logBuf[idx]; *p; p++) {
            if (*p == '"') out += '\\';
            out += *p;
        }
        out += "\"";
        if (i < logCount - 1) out += ",";
        out += "\n";
    }
    out += "]";
    req->send(200, "application/json", out);
}

//  /api/wifi/scan (GET) 
static void api_wifi_scan(AsyncWebServerRequest* req) {
    int n = WiFi.scanNetworks();
    JsonDocument doc;
    JsonArray arr = doc["networks"].to<JsonArray>();
    for (int i = 0; i < n; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["ssid"] = WiFi.SSID(i);
        o["rssi"] = WiFi.RSSI(i);
        o["enc"]  = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

//  WebSocket

static void ws_event(AsyncWebSocket* server, AsyncWebSocketClient* client,
                     AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WS] client #%u connected\n", client->id());
        // сразу шлём состояние
        webserver_push_state();
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] client #%u disconnected\n", client->id());
    } else if (type == WS_EVT_DATA) {
        // Приём команд по WS
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if (info->opcode == WS_TEXT) {
            char buf[512];
            size_t l = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
            memcpy(buf, data, l);
            buf[l] = 0;
            Serial.printf("[WS] rx: %s\n", buf);
            // Можно парсить команды
        }
    }
}

void webserver_push_state() {
    JsonDocument doc;
    doc["type"]   = "state";
    doc["page"]   = pages_current();
    doc["pages"]  = pages_total();
    doc["linked"] = comms_is_linked();
    doc["rssi"]   = WiFi.RSSI();
    doc["heap"]   = ESP.getFreeHeap();

    String out;
    serializeJson(doc, out);
    ws.textAll(out);
}

//  OTA

static void ota_upload(AsyncWebServerRequest* req, String filename,
                       size_t index, uint8_t* data, size_t len, bool final) {
    if (index == 0) {
        Serial.printf("[OTA] start: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
            req->send(500, "text/plain", "OTA begin failed");
            return;
        }
    }
    if (len) {
        if (Update.write(data, len) != len) {
            Update.printError(Serial);
        }
    }
    if (final) {
        if (Update.end(true)) {
            Serial.printf("[OTA] done: %u bytes\n", index + len);
        } else {
            Update.printError(Serial);
        }
        req->send(200, "text/plain", "OK, rebooting...");
        delay(500);
        ESP.restart();
    }
}

//  Отдача файлов из LittleFS

static bool serve_file(AsyncWebServerRequest* req, const char* path) {
    String p = path;
    if (p == "/") p = "/index.html";
    if (!LittleFS.exists(p)) return false;
    req->send(LittleFS, p, "text/html");
    return true;
}

//  Настройка маршрутов

static void setup_routes() {
    // Статика
    httpServer.serveStatic("/", LittleFS, "/");

    // API
    httpServer.on("/api/status",   HTTP_GET,  api_status);
    httpServer.on("/api/data",     HTTP_GET,  api_data);
    httpServer.on("/api/data",     HTTP_POST, [](AsyncWebServerRequest* r){},
                  NULL, api_data_post);
    httpServer.on("/api/settings", HTTP_GET,  api_settings_get);
    httpServer.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest* r){},
                  NULL, api_settings_post);
    httpServer.on("/api/control",  HTTP_POST, [](AsyncWebServerRequest* r){},
                  NULL, api_control_post);
    httpServer.on("/api/pages",    HTTP_GET,  api_pages_get);
    httpServer.on("/api/pages",    HTTP_POST, [](AsyncWebServerRequest* r){},
                  NULL, api_pages_post);
    httpServer.on("/api/buttons",  HTTP_GET,  api_buttons_get);
    httpServer.on("/api/buttons",  HTTP_POST, [](AsyncWebServerRequest* r){},
                  NULL, api_buttons_post);
    httpServer.on("/api/logs",     HTTP_GET,  api_logs_get);
    httpServer.on("/api/wifi/scan",HTTP_GET,  api_wifi_scan);

    // OTA
    httpServer.on("/api/ota", HTTP_POST,
        [](AsyncWebServerRequest* r){},
        ota_upload);

    // WebSocket
    ws.onEvent(ws_event);
    httpServer.addHandler(&ws);

    // Captive portal: любой неизвестный путь → setup.html (если AP)
    httpServer.onNotFound([](AsyncWebServerRequest* req) {
        if (apMode) {
            req->send(LittleFS, "/setup.html", "text/html");
        } else {
            req->send(404, "text/plain", "Not found");
        }
    });
}

//  Инициализация

void webserver_init() {
    bootMs = millis();

    // LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("[FS] LittleFS mount failed!");
    } else {
        Serial.println("[FS] LittleFS mounted");
    }

    // WiFi
    bool staOk = try_sta();
    if (!staOk) start_ap();

    // Отметки для boot-экрана
    extern void boot_check_wifi(bool ok);
    boot_check_wifi(staOk || apMode);

    // Маршруты + сервер
    setup_routes();
    httpServer.begin();
    Serial.printf("[WEB] server on %s:%d\n", staIp.c_str(), HTTP_PORT);

    // Отметка «веб поднялся»
    extern void boot_check_web(bool ok);
    boot_check_web(true);

    webserver_push_log("webserver started");
}

//  Апдейт (DNS для captive portal)

void webserver_update() {
    if (apMode) {
        dnsServer.processNextRequest();
    }

    // Периодический push состояния в WS (раз в секунду)
    static uint32_t lastPush = 0;
    if (millis() - lastPush > 1000) {
        lastPush = millis();
        webserver_push_state();
    }

    ws.cleanupClients();
}

//  Геттеры

String webserver_get_ip()      { return staIp; }
bool   webserver_is_ap_mode()  { return apMode; }
String webserver_get_ssid()    { return apMode ? String(AP_SSID) : WiFi.SSID(); }
int    webserver_get_rssi()    { return WiFi.RSSI(); }

void webserver_restart() { ESP.restart(); }
