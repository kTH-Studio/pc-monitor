#include "comms.h"
#include "pages.h"
#include "input.h"
#include "settings.h"
#include "lang.h"
#include "display.h"
#include <ArduinoJson.h>
#include <WiFi.h>

//  Состояние
 
static WiFiServer tcpServer(COMMS_PORT);
static WiFiClient tcpClient;

static CommChannel lastChannel = CH_NONE;
static uint32_t    lastRxMs    = 0;
static bool        linked      = false;

static char rxBuf[COMMS_BUF_SIZE];
static size_t rxLen = 0;

 
//  Инициализация
 
void comms_init() {
    Serial.begin(SERIAL_BAUD);
    tcpServer.begin();
    tcpServer.setNoDelay(true);
    Serial.printf("[COMMS] TCP server on port %d\n", COMMS_PORT);
}

 
//  Проверка связи (если данные не приходят > DATA_TIMEOUT_MS)
 
bool comms_is_linked() {
    if (!linked) return false;
    if (millis() - lastRxMs > DATA_TIMEOUT_MS) {
        linked = false;
        return false;
    }
    return true;
}

CommChannel comms_channel()    { return lastChannel; }
uint32_t    comms_last_rx_ms() { return lastRxMs; }

 
//  Парсинг JSON
 
static void parse_json(const char* json, size_t len) {
        // ===== Время с ПК =====
    if (doc["time"].is<uint32_t>()) {
        extern void clock_set_from_pc(uint32_t, int8_t);
        uint32_t t = doc["time"];
        int8_t tz  = doc["tz"] | 3;
        clock_set_from_pc(t, tz);
    }
    
    // Динамический документ (большой пакет)
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json, len);
    if (err) {
        Serial.printf("[COMMS] JSON error: %s\n", err.c_str());
        return;
    }

    //  GPU 
    if (doc["gpu"].is<JsonObject>()) {
        JsonObject o = doc["gpu"];
        g_gpu.present = o["present"] | false;
        if (g_gpu.present) {
            strlcpy(g_gpu.name, o["name"] | "", sizeof(g_gpu.name));
            g_gpu.load       = o["load"] | 0;
            g_gpu.temp       = o["temp"] | 0;
            g_gpu.vramUsed   = o["vram_used"] | 0.0f;
            g_gpu.vramTotal  = o["vram_total"] | 0.0f;
            g_gpu.power      = o["power"] | 0;
            g_gpu.fan        = o["fan"] | 0;
            strlcpy(g_gpu.driver, o["driver"] | "", sizeof(g_gpu.driver));
        }
    }

    //  CPU 
    if (doc["cpu"].is<JsonObject>()) {
        JsonObject o = doc["cpu"];
        g_cpu.present = o["present"] | false;
        if (g_cpu.present) {
            strlcpy(g_cpu.name, o["name"] | "", sizeof(g_cpu.name));
            g_cpu.load    = o["load"] | 0;
            g_cpu.temp    = o["temp"] | 0;
            g_cpu.freq    = o["freq"] | 0;
            g_cpu.power   = o["power"] | 0;
            g_cpu.coreMax = o["core_max"] | 0;
            g_cpu.fan     = o["fan"] | 0;
            g_cpu.vcore   = o["vcore"] | 0;
        }
    }

    //  RAM 
    if (doc["ram"].is<JsonObject>()) {
        JsonObject o = doc["ram"];
        g_ram.present = o["present"] | false;
        if (g_ram.present) {
            strlcpy(g_ram.name, o["name"] | "RAM", sizeof(g_ram.name));
            g_ram.load     = o["load"] | 0;
            g_ram.used     = o["used"] | 0.0f;
            g_ram.total    = o["total"] | 0.0f;
            g_ram.freq     = o["freq"] | 0;
            strlcpy(g_ram.type, o["type"] | "", sizeof(g_ram.type));
            g_ram.temp     = o["temp"] | 0;
            g_ram.voltage  = o["voltage"] | 0;
            g_ram.dualChannel = o["dual"] | false;
        }
    }

    //  Диски 
    if (doc["disks"].is<JsonArray>()) {
        JsonArray arr = doc["disks"];
        g_diskCount = 0;
        for (JsonObject o : arr) {
            if (g_diskCount >= 4) break;
            DiskData& d = g_disks[g_diskCount];
            d.present = true;
            d.letter  = (o["letter"] | "C")[0];
            strlcpy(d.name, o["name"] | "", sizeof(d.name));
            d.load      = o["load"] | 0;
            d.temp      = o["temp"] | 0;
            d.used      = o["used"] | 0.0f;
            d.total     = o["total"] | 0.0f;
            d.maxSpeed  = o["max_speed"] | 0;
            strlcpy(d.type, o["type"] | "", sizeof(d.type));
            d.readRate  = o["read_rate"] | 0;
            d.writeRate = o["write_rate"] | 0;
            d.lifeLeft  = o["life_left"] | 100;
            d.activity  = o["activity"] | 0;
            g_diskCount++;
        }
    }

    //  Сеть 
    if (doc["net"].is<JsonObject>()) {
        JsonObject o = doc["net"];
        g_net.present = o["present"] | false;
        if (g_net.present) {
            strlcpy(g_net.adapter, o["adapter"] | "", sizeof(g_net.adapter));
            g_net.downMbit   = o["down_mbit"] | 0.0f;
            g_net.upMbit     = o["up_mbit"] | 0.0f;
            g_net.totalDownGB= o["total_down_gb"] | 0.0f;
            g_net.totalUpGB  = o["total_up_gb"] | 0.0f;
            strlcpy(g_net.ip, o["ip"] | "", sizeof(g_net.ip));
            g_net.linkSpeed  = o["link_speed"] | 100;   // fallback 100 Mbit
            // Канал
            float loadMbit = g_net.downMbit + g_net.upMbit;
            uint16_t ls = g_net.linkSpeed > 0 ? g_net.linkSpeed : 100;
            uint8_t pct = (uint8_t)(loadMbit * 100.0f / ls);
            if (pct > 100) pct = 100;
            g_net.channelLoad = pct;
        }
    }

    //  Пинг 
    if (doc["ping"].is<JsonObject>()) {
        JsonObject o = doc["ping"];
        g_ping.present = o["present"] | false;
        if (g_ping.present) {
            g_ping.avgMs = o["avg"] | 0;
            g_ping.count = 0;
            if (o["targets"].is<JsonArray>()) {
                for (JsonObject t : o["targets"].as<JsonArray>()) {
                    if (g_ping.count >= 5) break;
                    strlcpy(g_ping.targets[g_ping.count].name,
                            t["name"] | "", sizeof(g_ping.targets[g_ping.count].name));
                    g_ping.targets[g_ping.count].ms = t["ms"] | 0;
                    g_ping.count++;
                }
            }
        }
    }

    //  Утечка 
    if (doc["leak"].is<JsonObject>()) {
        JsonObject o = doc["leak"];
        g_leak.present = o["present"] | false;
        g_leak.count = 0;
        g_leak.maxDeltaMB = 0;
        if (o["items"].is<JsonArray>()) {
            for (JsonObject it : o["items"].as<JsonArray>()) {
                if (g_leak.count >= 5) break;
                strlcpy(g_leak.items[g_leak.count].name,
                        it["name"] | "", sizeof(g_leak.items[g_leak.count].name));
                g_leak.items[g_leak.count].deltaMB = it["delta_mb"] | 0;
                if (g_leak.items[g_leak.count].deltaMB > g_leak.maxDeltaMB) {
                    g_leak.maxDeltaMB = g_leak.items[g_leak.count].deltaMB;
                }
                g_leak.count++;
            }
        }
    }

    //  Климат (с ESP32, но на всякий случай) 
    // (сенсоры обновляются в sensors.cpp — тут не трогаем)

    //  FPS 
    if (doc["fps"].is<JsonObject>()) {
        JsonObject o = doc["fps"];
        FpsData f;
        f.present  = o["present"] | false;
        if (f.present) {
            strlcpy(f.game, o["game"] | "", sizeof(f.game));
            f.current  = o["current"] | 0;
            f.minFps   = o["min"] | 0;
            f.maxFps   = o["max"] | 0;
            f.dropPct  = o["drop_pct"] | 0;
            f.gpuLoad  = o["gpu_load"] | 0;
            f.cpuLoad  = o["cpu_load"] | 0;
        }
        fps_set(f);
    }

    //  Медиа 
    if (doc["media"].is<JsonObject>()) {
        JsonObject o = doc["media"];
        MediaData m;
        m.present = o["present"] | false;
        if (m.present) {
            m.playing = o["playing"] | false;
            strlcpy(m.app,    o["app"] | "",    sizeof(m.app));
            strlcpy(m.artist, o["artist"] | "", sizeof(m.artist));
            strlcpy(m.track,  o["track"] | "",  sizeof(m.track));
            strlcpy(m.album,  o["album"] | "",  sizeof(m.album));
            m.posSec  = o["pos"] | 0;
            m.durSec  = o["dur"] | 0;
            m.updated = o["updated"] | false;
        }
        media_set(m);
    }

    //  Нотификации 
    if (doc["notify"].is<JsonArray>()) {
        for (JsonObject o : doc["notify"].as<JsonArray>()) {
            NotifyData n;
            strlcpy(n.app,   o["app"] | "",   sizeof(n.app));
            n.count = o["count"] | 1;
            strlcpy(n.title, o["title"] | "", sizeof(n.title));
            strlcpy(n.body,  o["body"] | "",  sizeof(n.body));
            strlcpy(n.time,  o["time"] | "",  sizeof(n.time));
            notify_push(n);
        }
    }

    //  Команды на ESP (с веба ПК, если нужно) 
    // (опционально)

    // ===== Время с ПК =====
    if (doc["time"].is<uint32_t>()) {
        extern void clock_set_from_pc(uint32_t, int8_t);
        uint32_t t = doc["time"];
        int8_t   tz = doc["tz"] | 3;
        clock_set_from_pc(t, tz);
    }

    // Обновить состояние связи
    lastRxMs = millis();
    linked   = true;
}

 
//  Приём по Serial
 
static void poll_serial() {
    while (Serial.available()) {
        char c = Serial.read();

        // Конец пакета: \n
        if (c == '\n') {
            rxBuf[rxLen] = 0;
            if (rxLen > 0) {
                lastChannel = CH_SERIAL;
                parse_json(rxBuf, rxLen);
            }
            rxLen = 0;
            continue;
        }

        if (rxLen < COMMS_BUF_SIZE - 1) {
            rxBuf[rxLen++] = c;
        } else {
            rxLen = 0;   // переполнение — сбрасываем
        }
    }
}

 
//  Приём по TCP
 
static void poll_tcp() {
    // Новый клиент
    if (tcpServer.hasClient()) {
        if (tcpClient && tcpClient.connected()) {
            tcpClient.stop();
        }
        tcpClient = tcpServer.accept();
        tcpClient.setNoDelay(true);
        Serial.println("[COMMS] TCP client connected");
    }

    if (!tcpClient || !tcpClient.connected()) return;

    while (tcpClient.available()) {
        char c = tcpClient.read();
        if (c == '\n') {
            rxBuf[rxLen] = 0;
            if (rxLen > 0) {
                lastChannel = CH_TCP;
                parse_json(rxBuf, rxLen);
            }
            rxLen = 0;
            continue;
        }
        if (rxLen < COMMS_BUF_SIZE - 1) {
            rxBuf[rxLen++] = c;
        } else {
            rxLen = 0;
        }
    }
}

 
//  Приём по HTTP (вызывается из webserver)
 
void comms_handle_http_body(const uint8_t* data, size_t len) {
    if (len == 0 || len >= COMMS_BUF_SIZE) return;
    memcpy(rxBuf, data, len);
    rxBuf[len] = 0;
    lastChannel = CH_HTTP;
    parse_json(rxBuf, len);
}

 
//  Общий апдейт
 
void comms_update() {
    poll_serial();
    poll_tcp();

    // Проверка таймаута
    if (linked && (millis() - lastRxMs > DATA_TIMEOUT_MS)) {
        linked = false;
        Serial.println("[COMMS] link lost");
    }
}

 
//  Отправка команд на ПК
 
static void send_json(const char* json) {
    size_t len = strlen(json);

    // Serial
    Serial.write((const uint8_t*)json, len);
    Serial.write('\n');

    // TCP
    if (tcpClient && tcpClient.connected()) {
        tcpClient.write((const uint8_t*)json, len);
        tcpClient.write('\n');
    }
    // HTTP — ПК сам опрашивает ESP, поэтому команды через HTTP не шлём
}

void comms_send_power(PowerAction act) {
    const char* cmd = "none";
    switch (act) {
        case PWR_SLEEP:  cmd = "sleep";  break;
        case PWR_REBOOT: cmd = "reboot"; break;
        case PWR_OFF:    cmd = "off";    break;
        default: return;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"cmd\":\"power\",\"action\":\"%s\"}", cmd);
    send_json(buf);
    Serial.printf("[COMMS] sent power: %s\n", cmd);
}

void comms_send_media(const char* cmd) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"cmd\":\"media\",\"action\":\"%s\"}", cmd);
    send_json(buf);
}

void comms_send_lang(Lang l) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"cmd\":\"lang\",\"value\":%d}", (int)l);
    send_json(buf);
}

void comms_send_reset(const char* what) {
    if (!what) what = "counters";
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"cmd\":\"reset\",\"what\":\"%s\"}", what);
    send_json(buf);
    Serial.printf("[COMMS] sent reset: %s\n", what);
}