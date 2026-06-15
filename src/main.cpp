/*
 * 冰箱PID温度控制系统
 * 基于ESP32-S3 + FreeRTOS + PID + OLED UI
 * 集成 actuator / state_machine
 */

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_SHT4x.h>
#include <GyverEncoder.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <PID_v1.h>
#include <PID_AutoTune_v0.h>
#include <Preferences.h>
#include "actuator.h"
#include "state_machine.h"

extern SystemState g_systemState; // 定义于 state_machine.cpp

// ==================== 引脚定义（仅 main.cpp 独有） ====================
// RELAY/LED/BUZZER 引脚见 actuator.h，此处不重复定义
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9
#define ENC_A_PIN 1
#define ENC_B_PIN 2
#define ENC_SW_PIN 3
#define PWM_COMPRESSOR_PIN 10
#define PWM_FAN_PIN 11

// ==================== OLED 和编码器对象 ====================
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, -1);
Encoder encoder(ENC_A_PIN, ENC_B_PIN, ENC_SW_PIN, TYPE1);

// ==================== 传感器对象 ====================
Adafruit_ADS1115 ads;
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

// ==================== FreeRTOS 同步原语 ====================
SemaphoreHandle_t i2cMutex = NULL;
SemaphoreHandle_t setpointMutex = NULL;
SemaphoreHandle_t displayDataMutex = NULL;
SemaphoreHandle_t pwmMutex = NULL;
QueueHandle_t temperatureQueue = NULL;
QueueHandle_t pwmOutputQueue = NULL;
QueueHandle_t uiEventQueue = NULL;
QueueHandle_t alarmQueue = NULL;

// ==================== 温度数据 ====================
float g_freezerTemp = -999.0;
float g_freshTemp = -999.0;
float g_filteredFreezerTemp = -999.0;
float g_filteredFreshTemp = -999.0;
float g_sht40Temp = -999.0;
float g_sht40Humidity = -999.0;
uint8_t g_compressorPWM = 0;
uint8_t g_fanPWM = 0;
double g_freezerSetpoint = -18.0;
double g_freshSetpoint = 4.0;

// ==================== PID 控制 ====================
// 注意：制冷系统需要用 REVERSE 方向
// REVERSE: 输出↑ → 输入↓（压缩机功率越大，温度越低）
double Kp_freezer = 2.0, Ki_freezer = 5.0, Kd_freezer = 1.0;
double Input_freezer, Output_freezer, Setpoint_freezer;
PID myPID_freezer(&Input_freezer, &Output_freezer, &Setpoint_freezer,
                  Kp_freezer, Ki_freezer, Kd_freezer, REVERSE); // 制冷需要 REVERSE

double Kp_fresh = 1.0, Ki_fresh = 0.5, Kd_fresh = 0.1;
double Input_fresh, Output_fresh, Setpoint_fresh;
PID myPID_fresh(&Input_fresh, &Output_fresh, &Setpoint_fresh,
                Kp_fresh, Ki_fresh, Kd_fresh, REVERSE); // 制冷需要 REVERSE

// ==================== NTC 参数 ====================
const float NTC_BETA = 3950.0;
const float NTC_R0 = 100000.0;
const float NTC_T0 = 25.0 + 273.15;
const float R_SERIES = 100000.0;
const float ADC_VREF = 3.3;
float EMA_ALPHA = 0.2;
bool g_firstReadingFreezer = true;
bool g_firstReadingFresh = true;

// ==================== OLED UI 状态 ====================
typedef enum
{
    PAGE_MAIN,
    PAGE_SENSORS,
    PAGE_PID,
    PAGE_MENU,
    PAGE_EDIT
} UIPage;

UIPage currentPage = PAGE_MAIN;
int selectedMenuItem = 0;
float editingSetpoint = -18.0;
bool editingFreezer = true; // true=冷冻, false=冷藏

const char *menuItems[] = {"Set Freezer", "Set Fresh", "Start Tune", "Back"};
const int numMenuItems = 4;

// ==================== 函数声明 ====================
float calculateNTCTemperature(int16_t adcValue);
float applyEMAFilter(float *filtered, float current, bool *firstFlag);
void sensorTask(void *pvParameters);
void controlTask(void *pvParameters);
void pwmOutputTask(void *pvParameters);
void encoderInputTask(void *pvParameters);
void uiTask(void *pvParameters);
void actuatorTask(void *pvParameters);
void stateMachineTask(void *pvParameters);

// UI 绘制函数
void handleEncoderEvent(int event);
void drawMainPage();
void drawSensorsPage();
void drawPidPage();
void drawMenuPage();
void drawEditPage();

// ==================== 辅助函数 ====================
float calculateNTCTemperature(int16_t adcValue)
{
    float voltage = adcValue * (4.096 / 32767.0);
    if (voltage < 0.01)
        voltage = 0.01;
    if (voltage > ADC_VREF - 0.01)
        voltage = ADC_VREF - 0.01;

    // 电路: 3.3V → 100K(R_SERIES) → ADC → NTC → GND
    float ntcResistance = R_SERIES * voltage / (ADC_VREF - voltage);

    if (ntcResistance > 0)
    {
        double logArg = (double)ntcResistance / NTC_R0;
        if (logArg > 0)
        {
            float steinhart = log(logArg) / NTC_BETA + (1.0 / NTC_T0);
            float temperature = (1.0 / steinhart) - 273.15;
            if (temperature < -50.0 || temperature > 150.0)
                return -998.0;
            return temperature;
        }
    }
    return -999.0;
}

float applyEMAFilter(float *filtered, float current, bool *firstFlag)
{
    if (*firstFlag)
    {
        *filtered = current;
        *firstFlag = false;
    }
    else
    {
        *filtered = EMA_ALPHA * current + (1.0 - EMA_ALPHA) * (*filtered);
    }
    return *filtered;
}

// ==================== Setup ====================
void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n========== 冰箱PID控制系统 ==========");
    Serial.println("基于ESP32-S3 + FreeRTOS");
    Serial.printf("初始内存: %u bytes\n", ESP.getFreeHeap());

    // I2C
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);

    // 互斥锁
    i2cMutex = xSemaphoreCreateMutex();
    setpointMutex = xSemaphoreCreateMutex();
    displayDataMutex = xSemaphoreCreateMutex();
    pwmMutex = xSemaphoreCreateMutex();

    // 队列
    temperatureQueue = xQueueCreate(5, sizeof(float) * 2);
    pwmOutputQueue = xQueueCreate(5, sizeof(uint8_t) * 2);
    uiEventQueue = xQueueCreate(10, sizeof(int));
    alarmQueue = xQueueCreate(5, 64);

    // OLED
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        if (!u8g2.begin())
            Serial.println("错误: OLED初始化失败!");
        xSemaphoreGive(i2cMutex);
    }

    // ADS1115
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        ads.setGain(GAIN_ONE);
        if (!ads.begin(0x48))
            Serial.println("错误: ADS1115初始化失败!");
        xSemaphoreGive(i2cMutex);
    }

    // SHT40
    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        if (!sht4.begin(&Wire))
            Serial.println("错误: SHT40初始化失败!");
        xSemaphoreGive(i2cMutex);
    }

    // PWM
    ledcSetup(0, 5000, 8);
    ledcAttachPin(PWM_COMPRESSOR_PIN, 0);
    ledcSetup(1, 5000, 8);
    ledcAttachPin(PWM_FAN_PIN, 1);

    // PID
    myPID_freezer.SetMode(AUTOMATIC);
    myPID_freezer.SetSampleTime(100);
    myPID_freezer.SetOutputLimits(0, 255);

    myPID_fresh.SetMode(AUTOMATIC);
    myPID_fresh.SetSampleTime(100);
    myPID_fresh.SetOutputLimits(0, 255);

    // 初始化库（actuator 由 state_machine 内部调用 Actuator_Init）
    StateMachine_Init();

    // 创建 FreeRTOS 任务
    xTaskCreate(sensorTask, "SensorTask", 4096, NULL, 3, NULL);
    xTaskCreate(controlTask, "ControlTask", 4096, NULL, 2, NULL);
    xTaskCreate(pwmOutputTask, "PWMTask", 2048, NULL, 2, NULL);
    xTaskCreate(encoderInputTask, "EncoderTask", 2048, NULL, 4, NULL);
    xTaskCreate(uiTask, "UITask", 4096, NULL, 1, NULL);
    xTaskCreate(actuatorTask, "ActuatorTask", 2048, NULL, 2, NULL);
    xTaskCreate(stateMachineTask, "StateMachineTask", 3072, NULL, 2, NULL);

    Serial.println("系统初始化完成!");
    Serial.printf("剩余内存: %u bytes\n", ESP.getFreeHeap());
}

// ==================== 主循环 ====================
void loop()
{
    vTaskDelay(portMAX_DELAY);
}

// ==================== 传感器任务 ====================
void sensorTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100);

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            int16_t adc0 = ads.readADC_SingleEnded(0);
            int16_t adc1 = ads.readADC_SingleEnded(1);

            sensors_event_t humidity, temp;
            bool sht40ok = sht4.getEvent(&humidity, &temp);

            xSemaphoreGive(i2cMutex);

            float temp0 = calculateNTCTemperature(adc0);
            float temp1 = calculateNTCTemperature(adc1);

            if (temp0 > -900.0)
            {
                g_filteredFreezerTemp = applyEMAFilter(&g_filteredFreezerTemp,
                                                       temp0, &g_firstReadingFreezer);
            }
            if (temp1 > -900.0)
            {
                g_filteredFreshTemp = applyEMAFilter(&g_filteredFreshTemp,
                                                     temp1, &g_firstReadingFresh);
            }

            if (xSemaphoreTake(displayDataMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                g_freezerTemp = g_filteredFreezerTemp;
                g_freshTemp = g_filteredFreshTemp;
                if (sht40ok)
                {
                    g_sht40Temp = temp.temperature;
                    g_sht40Humidity = humidity.relative_humidity;
                }
                xSemaphoreGive(displayDataMutex);
            }

            float temps[2] = {g_filteredFreezerTemp, g_filteredFreshTemp};
            xQueueSend(temperatureQueue, temps, 0);
        }
    }
}

// ==================== 控制任务 ====================
void controlTask(void *pvParameters)
{
    float temps[2];

    for (;;)
    {
        if (xQueueReceive(temperatureQueue, temps, portMAX_DELAY) == pdPASS)
        {
            Input_freezer = temps[0];
            Input_fresh = temps[1];

            if (xSemaphoreTake(setpointMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                Setpoint_freezer = g_freezerSetpoint;
                Setpoint_fresh = g_freshSetpoint;
                xSemaphoreGive(setpointMutex);
            }

            myPID_freezer.Compute();
            myPID_fresh.Compute();

            if (xSemaphoreTake(pwmMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                g_compressorPWM = (uint8_t)Output_freezer;
                g_fanPWM = (uint8_t)Output_fresh;
                xSemaphoreGive(pwmMutex);
            }

            uint8_t pwmValues[2] = {g_compressorPWM, g_fanPWM};
            xQueueSend(pwmOutputQueue, pwmValues, 0);
        }
    }
}

// ==================== PWM 输出任务 ====================
void pwmOutputTask(void *pvParameters)
{
    uint8_t pwmValues[2];

    for (;;)
    {
        if (xQueueReceive(pwmOutputQueue, pwmValues, portMAX_DELAY) == pdPASS)
        {
            ledcWrite(0, pwmValues[0]);
            ledcWrite(1, pwmValues[1]);
        }
    }
}

// ==================== 编码器输入任务 ====================
void encoderInputTask(void *pvParameters)
{
    encoder.setType(TYPE1);
    encoder.setTickMode(AUTO);

    for (;;)
    {
        encoder.tick();

        int event = 0;
        if (encoder.isTurn())
            event = encoder.isRight() ? 2 : 1;
        else if (encoder.isClick())
            event = 3;
        else if (encoder.isDouble())
            event = 4;

        if (event != 0)
            xQueueSend(uiEventQueue, &event, 0);

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ==================== UI 任务（完整 5 页 OLED 界面） ====================
void uiTask(void *pvParameters)
{
    int event;

    for (;;)
    {
        if (xQueueReceive(uiEventQueue, &event, pdMS_TO_TICKS(100)) == pdPASS)
            handleEncoderEvent(event);

        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            u8g2.clearBuffer();

            switch (currentPage)
            {
            case PAGE_MAIN:
                drawMainPage();
                break;
            case PAGE_SENSORS:
                drawSensorsPage();
                break;
            case PAGE_PID:
                drawPidPage();
                break;
            case PAGE_MENU:
                drawMenuPage();
                break;
            case PAGE_EDIT:
                drawEditPage();
                break;
            }

            u8g2.sendBuffer();
            xSemaphoreGive(i2cMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ==================== 编码器事件处理 ====================
void handleEncoderEvent(int event)
{
    // event: 1=左转, 2=右转, 3=单击, 4=双击
    switch (currentPage)
    {
    case PAGE_MAIN:
        if (event == 1)
            currentPage = PAGE_PID;
        else if (event == 2)
            currentPage = PAGE_SENSORS;
        else if (event == 3)
        {
            currentPage = PAGE_MENU;
            selectedMenuItem = 0;
        }
        break;

    case PAGE_SENSORS:
        if (event == 1)
            currentPage = PAGE_MAIN;
        else if (event == 2)
            currentPage = PAGE_PID;
        else if (event == 3)
        {
            currentPage = PAGE_MENU;
            selectedMenuItem = 0;
        }
        break;

    case PAGE_PID:
        if (event == 1)
            currentPage = PAGE_SENSORS;
        else if (event == 2)
            currentPage = PAGE_MAIN;
        else if (event == 3)
        {
            currentPage = PAGE_MENU;
            selectedMenuItem = 0;
        }
        break;

    case PAGE_MENU:
        if (event == 1)
        {
            selectedMenuItem--;
            if (selectedMenuItem < 0)
                selectedMenuItem = numMenuItems - 1;
        }
        else if (event == 2)
        {
            selectedMenuItem++;
            if (selectedMenuItem >= numMenuItems)
                selectedMenuItem = 0;
        }
        else if (event == 3)
        {
            switch (selectedMenuItem)
            {
            case 0: // Set Freezer
                editingSetpoint = g_freezerSetpoint;
                editingFreezer = true;
                currentPage = PAGE_EDIT;
                break;
            case 1: // Set Fresh
                editingSetpoint = g_freshSetpoint;
                editingFreezer = false;
                currentPage = PAGE_EDIT;
                break;
            case 2: // Start Tune
                Serial.println("开始PID自动调谐...");
                currentPage = PAGE_MAIN;
                break;
            case 3: // Back
                currentPage = PAGE_MAIN;
                break;
            }
        }
        else if (event == 4)
        {
            currentPage = PAGE_MAIN;
        }
        break;

    case PAGE_EDIT:
        if (event == 1)
        {
            editingSetpoint -= 0.5;
            if (editingSetpoint < -30.0)
                editingSetpoint = -30.0;
        }
        else if (event == 2)
        {
            editingSetpoint += 0.5;
            if (editingSetpoint > 10.0)
                editingSetpoint = 10.0;
        }
        else if (event == 3)
        {
            // 保存设定温度
            if (xSemaphoreTake(setpointMutex, pdMS_TO_TICKS(100)) == pdTRUE)
            {
                if (editingFreezer)
                    g_freezerSetpoint = editingSetpoint;
                else
                    g_freshSetpoint = editingSetpoint;
                xSemaphoreGive(setpointMutex);
            }
            StateMachine_SetTargetTemp(g_freezerSetpoint);
            Serial.printf("设定温度已保存: %.1f°C (%s)\n",
                          editingSetpoint, editingFreezer ? "冷冻" : "冷藏");
            currentPage = PAGE_MENU;
        }
        else if (event == 4)
        {
            currentPage = PAGE_MENU;
        }
        break;
    }
}

// ==================== OLED 绘制函数 ====================
void drawMainPage()
{
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, "Fridge PID Control");

    // 读取温度数据（初始化为 NAN，防止互斥锁获取失败时显示随机值）
    float fTemp = NAN, rTemp = NAN;
    if (xSemaphoreTake(displayDataMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        fTemp = g_freezerTemp;
        rTemp = g_freshTemp;
        xSemaphoreGive(displayDataMutex);
    }

    u8g2.setFont(u8g2_font_fub20_tr);
    char tempStr[10];
    if (!isnan(fTemp))
    {
        dtostrf(fTemp, 5, 1, tempStr);
    }
    else
    {
        strcpy(tempStr, "N/A");
    }
    u8g2.drawStr(0, 35, tempStr);
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(60, 35, "°C");

    u8g2.setFont(u8g2_font_fub14_tr);
    dtostrf(rTemp, 4, 1, tempStr);
    u8g2.drawStr(0, 55, "Fresh:");
    u8g2.drawStr(50, 55, tempStr);
    u8g2.drawStr(95, 55, "°C");

    u8g2.setFont(u8g2_font_micro_tr);
    u8g2.drawStr(55, 63, "1/3  Turn");
}

void drawSensorsPage()
{
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, "Sensors Info");

    // 初始化为 NAN，防止互斥锁获取失败时显示随机值
    float fTemp = NAN, rTemp = NAN, sTemp = NAN, sHum = NAN;
    if (xSemaphoreTake(displayDataMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        fTemp = g_freezerTemp;
        rTemp = g_freshTemp;
        sTemp = g_sht40Temp;
        sHum = g_sht40Humidity;
        xSemaphoreGive(displayDataMutex);
    }

    char dataStr[30];
    snprintf(dataStr, sizeof(dataStr), "SHT: %.1fC %.0f%%", sTemp, sHum);
    u8g2.drawStr(0, 25, dataStr);

    snprintf(dataStr, sizeof(dataStr), "Freezer: %.1fC", fTemp);
    u8g2.drawStr(0, 40, dataStr);

    snprintf(dataStr, sizeof(dataStr), "Fresh: %.1fC", rTemp);
    u8g2.drawStr(0, 55, dataStr);

    u8g2.setFont(u8g2_font_micro_tr);
    u8g2.drawStr(55, 63, "2/3  Turn");
}

void drawPidPage()
{
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, "PID Parameters");

    // 初始化为 0，防止互斥锁获取失败时显示随机值
    uint8_t pwm = 0;
    if (xSemaphoreTake(pwmMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        pwm = g_compressorPWM;
        xSemaphoreGive(pwmMutex);
    }

    char pidStr[30];
    snprintf(pidStr, sizeof(pidStr), "Kp:%.1f Ki:%.1f", Kp_freezer, Ki_freezer);
    u8g2.drawStr(0, 25, pidStr);

    snprintf(pidStr, sizeof(pidStr), "Kd:%.1f", Kd_freezer);
    u8g2.drawStr(0, 40, pidStr);

    snprintf(pidStr, sizeof(pidStr), "PWM: %d%%", (int)(pwm * 100.0 / 255.0));
    u8g2.drawStr(0, 55, pidStr);

    const char *modeName = StateMachine_GetModeName(StateMachine_GetMode());
    u8g2.drawStr(0, 63, modeName);

    u8g2.setFont(u8g2_font_micro_tr);
    u8g2.drawStr(110, 63, "3/3");
}

void drawMenuPage()
{
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, "Settings");
    u8g2.drawLine(0, 12, 128, 12);

    float frSet, fhSet;
    if (xSemaphoreTake(setpointMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        frSet = g_freezerSetpoint;
        fhSet = g_freshSetpoint;
        xSemaphoreGive(setpointMutex);
    }

    for (int i = 0; i < numMenuItems; i++)
    {
        if (i == selectedMenuItem)
            u8g2.drawStr(0, 25 + i * 10, ">");

        char itemStr[30];
        switch (i)
        {
        case 0:
            snprintf(itemStr, sizeof(itemStr), "Freezer: %.1fC", frSet);
            break;
        case 1:
            snprintf(itemStr, sizeof(itemStr), "Fresh: %.1fC", fhSet);
            break;
        default:
            snprintf(itemStr, sizeof(itemStr), "%s", menuItems[i]);
            break;
        }
        u8g2.drawStr(10, 25 + i * 10, itemStr);
    }
}

void drawEditPage()
{
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 10, editingFreezer ? "Set Freezer Temp" : "Set Fresh Temp");
    u8g2.drawLine(0, 12, 128, 12);

    char tempStr[10];
    dtostrf(editingSetpoint, 5, 1, tempStr);
    char editStr[20];
    snprintf(editStr, sizeof(editStr), "[ %s ]", tempStr);
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(20, 35, editStr);

    u8g2.setFont(u8g2_font_micro_tr);
    u8g2.drawStr(0, 55, "Turn: +/- 0.5C");
    u8g2.drawStr(0, 63, "Click:Save  Double:Cancel");
}

// ==================== 执行器任务（LED 闪烁更新） ====================
void actuatorTask(void *pvParameters)
{
    for (;;)
    {
        Actuator_UpdateLEDs();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// ==================== 状态机任务 ====================
void stateMachineTask(void *pvParameters)
{
    bool wasAlarm = false;

    for (;;)
    {
        // 读取当前温度
        float freezerTemp, freshTemp;
        if (xSemaphoreTake(displayDataMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            freezerTemp = g_freezerTemp;
            freshTemp = g_freshTemp;
            xSemaphoreGive(displayDataMutex);
        }

        // 读取 PWM 值决定压缩机目标状态
        uint8_t pwm;
        if (xSemaphoreTake(pwmMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            pwm = g_compressorPWM;
            xSemaphoreGive(pwmMutex);
        }

        // PID 有输出 → 压缩机应该运行
        g_systemState.compressorOn = (pwm > 20);

        // 更新状态机
        bool doorOpen = false; // TODO: 实际接入门磁传感器
        StateMachine_Update(freezerTemp, freshTemp, doorOpen);

        // 温度异常报警（边沿触发 MQTT 通知）
        bool isAlarm = (freezerTemp > -10.0 || freezerTemp < -30.0);
        if (isAlarm && !wasAlarm)
        {
            char msg[64];
            snprintf(msg, sizeof(msg), "Freezer abnormal: %.1fC", freezerTemp);
            xQueueSend(alarmQueue, msg, 0);
        }
        else if (!isAlarm && wasAlarm)
        {
            xQueueSend(alarmQueue, "Alarm cleared", 0);
        }
        wasAlarm = isAlarm;

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
