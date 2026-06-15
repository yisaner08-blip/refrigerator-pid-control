/*
 * WiFi Web Server - 冰箱PID控制系统
 * 提供HTTP API接口供微信小程序控制
 */

#ifndef WIFI_WEBSERVER_H
#define WIFI_WEBSERVER_H

#include <Arduino.h>

// =================== 函数声明 ===================
void WiFiWebServer_Init();
void WiFiWebServer_Loop();
bool WiFiWebServer_IsConnected();
String WiFiWebServer_GetIP();
void handleRoot();

#endif
