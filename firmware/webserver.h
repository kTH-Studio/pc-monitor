#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "lang.h"

//  Веб-сервер (HTTP + WS + OTA + Captive Portal)

//  Инициализация 
void webserver_init();       // WiFi (STA→AP) + HTTP + WS + OTA
void webserver_update();     // DNS-обработчик для captive portal

//  Состояние 
String webserver_get_ip();
bool   webserver_is_ap_mode();
String webserver_get_ssid();
int    webserver_get_rssi();

//  WebSocket push 
void webserver_push_state();         // обновить всех клиентов
void webserver_push_log(const char* line);

//  Управление 
void webserver_restart();            // перезагрузка ESP
void webserver_ota_begin();          // подготовка к OTA
