/*
 * WiFi Web Server - 冰箱PID控制系统
 * 提供HTTP API接口供微信小程序控制
 */

#include "wifi_webserver.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <PID_v1.h>

#include "state_machine.h"
#include "actuator.h"
#include "web_interface.h"

// =================== 全局变量 ===================
WebServer webServer(80);

// =================== 外部变量引用 ===================
extern double g_freezerSetpoint;
extern float g_freezerTemp;
extern double Output_freezer;     // PID输出值
extern double Setpoint_freezer;   // PID设定值（关键！必须同步）
extern double Kp_freezer, Ki_freezer, Kd_freezer;  // PID参数
extern SystemMode currentMode;    // wokwi_main.cpp 使用的模式变量
extern SemaphoreHandle_t setpointMutex;

// =================== 外部 PID 对象引用 ===================
extern PID myPID_freezer;

// =================== 外部函数声明 ===================
extern String getTempHistoryJSON();

// =================== CORS 头设置函数 ===================
void addCORSHeaders() {
    webServer.sendHeader("Access-Control-Allow-Origin", "*");
    webServer.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    webServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// =================== API处理函数 ===================

// 处理 /api/status - 获取系统状态
void handleStatus() {
    addCORSHeaders();  // 添加 CORS 头
    
    DynamicJsonDocument doc(512);
    
    // 温度信息
    doc["freezer_temp"] = g_freezerTemp;
    doc["freezer_setpoint"] = g_freezerSetpoint;
    
    // 设备状态
    doc["compressor"] = Actuator_IsCompressorOn();  // 布尔值，方便小程序直接使用
    doc["fan"] = (int)Actuator_GetFanSpeed();
    doc["defrost"] = Actuator_IsDefrostOn();  // 布尔值
    
    // 模式信息（同时返回数字和名称，小程序直接使用 mode_code）
    SystemMode currentSysMode = StateMachine_GetMode();
    doc["mode_code"] = (int)currentSysMode;  // 0=制冷 1=除霜 2=节能 3=故障
    doc["mode"] = StateMachine_GetModeName(currentSysMode);
    
    // PID 输出
    doc["pid_output"] = Output_freezer;
    doc["uptime"] = millis() / 1000;
    
    String response;
    serializeJson(doc, response);
    
    webServer.send(200, "application/json", response);
}

// 处理 /api/setpoint - 设置温度
void handleSetSetpoint() {
    addCORSHeaders();
    
    DynamicJsonDocument doc(100);
    
    if (webServer.hasArg("plain")) {
        deserializeJson(doc, webServer.arg("plain"));
        
        double value = doc["value"];
        
        if (xSemaphoreTake(setpointMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_freezerSetpoint = value;
            Setpoint_freezer = value;  // 同步PID真正使用的设定值！
            xSemaphoreGive(setpointMutex);
        }
        
        DynamicJsonDocument resp(100);
        resp["success"] = true;
        resp["message"] = "温度设置成功";
        
        String response;
        serializeJson(resp, response);
        webServer.send(200, "application/json", response);
        
        Serial.printf("Web: 设定温度 -> %.1f°C\n", value);
    } else {
        webServer.send(400, "application/json", "{\"error\":\"请求为空\"}");
    }
}

// 处理 /api/mode - 设置模式
void handleSetMode() {
    addCORSHeaders();
    
    DynamicJsonDocument doc(100);
    
    if (webServer.hasArg("plain")) {
        deserializeJson(doc, webServer.arg("plain"));
        
        int mode = doc["mode"];
        SystemMode newMode = (SystemMode)mode;
        
        // 同时更新两个模式变量（确保 wokwi_main.cpp 和 state_machine 同步）
        currentMode = newMode;
        StateMachine_SetMode(newMode);
        
        DynamicJsonDocument resp(100);
        resp["success"] = true;
        resp["message"] = "模式设置成功";
        
        String response;
        serializeJson(resp, response);
        webServer.send(200, "application/json", response);
        
        Serial.printf("Web: 模式 -> %d (%s)\n", mode, StateMachine_GetModeName(newMode));
    } else {
        webServer.send(400, "application/json", "{\"error\":\"请求为空\"}");
    }
}

// 处理 /api/pid - 设置PID参数
void handleSetPID() {
    addCORSHeaders();
    
    DynamicJsonDocument doc(200);
    
    if (webServer.hasArg("plain")) {
        deserializeJson(doc, webServer.arg("plain"));
        
        double kp = doc["kp"];
        double ki = doc["ki"];
        double kd = doc["kd"];
        
        // 同步 PID 参数（库内部更新 + 全局变量同步）
        Kp_freezer = kp; Ki_freezer = ki; Kd_freezer = kd;
        myPID_freezer.SetTunings(kp, ki, kd);
        
        DynamicJsonDocument resp(100);
        resp["success"] = true;
        resp["message"] = "PID参数设置成功";
        
        String response;
        serializeJson(resp, response);
        webServer.send(200, "application/json", response);
        
        Serial.printf("Web: PID参数 Kp=%.2f Ki=%.2f Kd=%.2f\n", kp, ki, kd);
    } else {
        webServer.send(400, "application/json", "{\"error\":\"请求为空\"}");
    }
}

// 处理 /api/history - 获取温度历史
void handleHistory() {
    addCORSHeaders();  // 添加 CORS 头
    
    // 使用真实的环形缓冲区数据
    String response = getTempHistoryJSON();
    webServer.send(200, "application/json", response);
}

// 处理根路径 - 返回网页界面（直接从PROGMEM发送，避免堆碎片）
void handleRoot() {
    webServer.sendHeader("Connection", "close");
    webServer.send_P(200, "text/html", HTML_PAGE);
}

// 处理CORS预检请求
void handleCORS() {
    addCORSHeaders();  // 使用统一的 CORS 头设置函数
    webServer.send(204);
}

// =================== 初始化 ===================
void WiFiWebServer_Init() {
    // 配置API路由
    webServer.on("/", HTTP_GET, handleRoot);  // 根路径返回网页界面
    webServer.on("/api/status", HTTP_GET, handleStatus);
    webServer.on("/api/setpoint", HTTP_POST, handleSetSetpoint);
    webServer.on("/api/mode", HTTP_POST, handleSetMode);
    webServer.on("/api/pid", HTTP_POST, handleSetPID);
    webServer.on("/api/history", HTTP_GET, handleHistory);
    
    // CORS支持
    webServer.onNotFound([]() {
        if (webServer.method() == HTTP_OPTIONS) {
            handleCORS();
        } else {
            webServer.send(404, "application/json", "{\"error\":\"Not Found\"}");
        }
    });
    
    // 启动服务器
    webServer.begin();
    
    Serial.println("HTTP Web服务器已启动");
    Serial.print("SoftAP IP: ");
    Serial.println(WiFi.softAPIP());
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("STA IP: ");
        Serial.println(WiFi.localIP());
    }
    Serial.println("访问地址: http://<SoftAP_IP> 或 http://<STA_IP>");
}

// =================== 主循环 ===================
void WiFiWebServer_Loop() {
    webServer.handleClient();
}

// =================== 获取连接状态 ===================
bool WiFiWebServer_IsConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiWebServer_GetIP() {
    return WiFi.localIP().toString();
}
