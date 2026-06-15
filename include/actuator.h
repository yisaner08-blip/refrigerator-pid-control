/*
 * 执行器控制库 - 冰箱PID控制系统
 * 实现继电器控制、PWM风扇控制、蜂鸣器报警、LED状态指示
 */

#ifndef ACTUATOR_H
#define ACTUATOR_H

#include <Arduino.h>

// ==================== 执行器引脚定义 ====================
#define RELAY_COMPRESSOR_PIN  12  // 压缩机继电器
#define RELAY_DEFROST_PIN    13  // 除霜加热丝继电器
#define BUZZER_PIN           14  // 蜂鸣器
#define LED_RUN_PIN           15  // 运行指示灯(绿)
#define LED_STANDBY_PIN      16  // 待机指示灯(黄)
#define LED_FAULT_PIN        17  // 故障指示灯(红)
#define LED_COMPRESSOR_PIN   18  // 压缩机状态灯(蓝)

// ==================== 状态枚举 ====================
typedef enum {
    COMPRESSOR_OFF = 0,
    COMPRESSOR_ON = 1
} CompressorState;

typedef enum {
    DEFROST_OFF = 0,
    DEFROST_ON = 1
} DefrostState;

typedef enum {
    FAN_OFF = 0,
    FAN_LOW = 85,     // 33% PWM
    FAN_MEDIUM = 170,  // 66% PWM
    FAN_HIGH = 255     // 100% PWM
} FanSpeed;

typedef enum {
    BUZZER_OFF = 0,
    BUZZER_ON = 1
} BuzzerState;

typedef enum {
    LED_OFF = 0,
    LED_ON = 1,
    LED_BLINK_SLOW = 2,  // 慢闪(500ms)
    LED_BLINK_FAST = 3   // 快闪(200ms)
} LEDState;

// ==================== 压缩机保护参数 ====================
const unsigned long MIN_COMPRESSOR_OFF_TIME = 180000;  // 最小停机时间 3分钟
const unsigned long MIN_COMPRESSOR_ON_TIME = 300000;   // 最小运行时间 5分钟

// ==================== 函数声明 ====================

// 初始化执行器
void Actuator_Init();

// 压缩机控制
void Actuator_SetCompressor(CompressorState state);
bool Actuator_CanStartCompressor();  // 检查是否可以启动压缩机
bool Actuator_CanStopCompressor();   // 检查是否可以停止压缩机

// 除霜加热丝控制
void Actuator_SetDefrost(DefrostState state);

// 风扇控制
void Actuator_SetFanSpeed(FanSpeed speed);
void Actuator_SetFanPWM(uint8_t pwmValue);  // 直接设置PWM值

// 蜂鸣器控制
void Actuator_SetBuzzer(BuzzerState state);
void Actuator_Beep(unsigned int durationMs);  // 蜂鸣器响指定时间

// LED控制
void Actuator_SetLED(int ledId, LEDState state);
void Actuator_UpdateLEDs();  // 更新所有LED状态(需在loop中定期调用)

// 状态指示
void Actuator_ShowSystemStatus(int statusCode);  // 通过LED显示系统状态

// 报警功能
void Actuator_Alarm_TemperatureTooHigh(float currentTemp, float setpoint);
void Actuator_Alarm_TemperatureTooLow(float currentTemp, float setpoint);
void Actuator_Alarm_SensorError();
void Actuator_Alarm_DoorOpen(unsigned long doorOpenTime);

// 获取执行器状态
bool Actuator_IsCompressorOn();
bool Actuator_IsDefrostOn();
uint8_t Actuator_GetFanSpeed();
bool Actuator_IsBuzzerOn();

#endif // ACTUATOR_H
