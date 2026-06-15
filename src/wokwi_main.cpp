/*
 * 冰箱PID控制系统 - Wokwi仿真版本
 * 适用于Wokwi在线仿真器测试
 * 包含WiFi Web服务器功能
 * 注意：此版本使用模拟数据替代ADS1115和SHT40（Wokwi不支持）
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "pin_config.h"
#include "actuator.h"
#include "state_machine.h"
#include "sensors_sim.h"
#include "temp_history.h"
#include "pid_control.h"
#include "oled_display.h"
#include "input_handler.h"
#include "system_control.h"
#include "wifi_webserver.h"

// ========== WiFi 配置 ==========
const char *ssid = "FridgeControl";
const char *password = "12345678";
const char *sta_ssid = "Wokwi-GUEST";
const char *sta_password = "";

// ========== 初始化 ==========
void setup()
{
  Serial.begin(115200);
  Serial.println("Fridge PID Control System - Wokwi Simulation");
  Serial.println("============================================");

  Wire.begin(OLED_SDA, OLED_SCL);

  // 启动软AP模式
  Serial.println("Starting SoftAP...");
  WiFi.softAP(ssid, password);
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("SoftAP IP: ");
  Serial.println(apIP);

  // 尝试连接Wokwi WiFi（可选）
  Serial.print("Connecting to WiFi: ");
  Serial.println(sta_ssid);
  WiFi.begin(sta_ssid, sta_password);

  int wifi_retry = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_retry < 10)
  {
    delay(500);
    Serial.print(".");
    wifi_retry++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println();
    Serial.print("WiFi connected! STA IP: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println();
    Serial.println("STA WiFi connection failed (SoftAP still active).");
  }

  Serial.print("Web server will be at: http://");
  Serial.println(apIP);

  // 初始化OLED
  oled_init();

  // 初始化PID
  pid_init();

  // 初始化Wokwi仿真GPIO
  pinMode(COMPRESSOR_LED_PIN, OUTPUT);
  pinMode(DEFROST_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(COMPRESSOR_LED_PIN, LOW);
  digitalWrite(DEFROST_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Actuator_Init();

  // 创建互斥锁（WiFi WebServer POST请求使用）
  setpointMutex = xSemaphoreCreateMutex();

  // 初始化ADC
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // 初始化按钮
  input_init();

  delay(1000);
  Serial.println("System initialized successfully!");

  Serial.println("\n--- 交互方式说明 ---");
  Serial.println("方式1：在画面中用鼠标点击黄/橙/棕色按钮");
  Serial.println("方式2：在下方此终端内输入字母并按回车:");
  Serial.println("  输入 'a' -> 模拟A按钮 (向上/左)");
  Serial.println("  输入 'b' -> 模拟B按钮 (向下/右)");
  Serial.println("  输入 's' -> 模拟SW按钮 (进入菜单/确认)");
  Serial.println("  输入 't' -> 切换温度来源(模拟/NTC传感器)");
  Serial.println("--------------------\n");

  WiFiWebServer_Init();
  Serial.println("Web server initialized. Waiting for connections...");
}

// ========== 主循环 ==========
void loop()
{
  WiFiWebServer_Loop();
  simulateTemperature();
  handleEncoder();
  handleSerialInput();
  updatePID();
  controlActuators();
  checkSafety();
  updateSystemMode();
  updateOLED();

  // 每2秒输出调试信息
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 2000)
  {
    Serial.println("===== System Status =====");
    Serial.print("Temperature: ");
    Serial.print(g_freezerTemp, 2);
    Serial.println(" C");
    Serial.print("Target: ");
    Serial.print(Setpoint_freezer, 2);
    Serial.println(" C");
    Serial.print("PID Output: ");
    Serial.println(Output_freezer, 2);
    Serial.print("Compressor: ");
    Serial.println(Actuator_IsCompressorOn() ? "ON" : "OFF");
    Serial.print("Mode: ");
    switch (currentMode)
    {
    case MODE_COOLING:
      Serial.println("COOLING");
      break;
    case MODE_DEFROST:
      Serial.println("DEFROST");
      break;
    case MODE_ECO:
      Serial.println("ECO");
      break;
    case MODE_ERROR:
      Serial.println("ERROR");
      break;
    }
    Serial.println("========================");
    lastPrint = millis();
  }

  delay(10);
}
