/*
 * 执行器控制库 - 冰箱PID控制系统
 * 实现继电器控制、PWM风扇控制、蜂鸣器报警、LED状态指示
 */

#include "actuator.h"
#include "state_machine.h"

// ==================== 内部函数前向声明 ====================
void Actuator_SetCompressor(CompressorState state);
void Actuator_SetDefrost(DefrostState state);
void Actuator_SetFanSpeed(FanSpeed speed);
void Actuator_SetBuzzer(BuzzerState state);
void Actuator_SetLED(int ledId, LEDState state);

// ==================== 全局变量 ====================
CompressorState g_compressorState = COMPRESSOR_OFF;
DefrostState g_defrostState = DEFROST_OFF;
FanSpeed g_fanSpeed = FAN_OFF;
BuzzerState g_buzzerState = BUZZER_OFF;

// 注意：压缩机保护时间变量和逻辑已移至 state_machine.cpp
// 此处不再维护重复变量，改为调用 StateMachine_CanStartCompressor() 等接口

// LED状态
LEDState g_ledStates[4] = {LED_OFF, LED_OFF, LED_OFF, LED_OFF};
unsigned long g_ledBlinkTimers[4] = {0, 0, 0, 0};

// ==================== 初始化 ====================
void Actuator_Init() {
    // 初始化执行器引脚
    pinMode(RELAY_COMPRESSOR_PIN, OUTPUT);
    pinMode(RELAY_DEFROST_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_RUN_PIN, OUTPUT);
    pinMode(LED_STANDBY_PIN, OUTPUT);
    pinMode(LED_FAULT_PIN, OUTPUT);
    pinMode(LED_COMPRESSOR_PIN, OUTPUT);
    
    // 初始化PWM（已在main.cpp中初始化）
    // ledcSetup(0, 5000, 8);  // 通道0, 5kHz, 8-bit
    // ledcAttachPin(PWM_FAN_PIN, 0);
    
    // 关闭所有执行器
    Actuator_SetCompressor(COMPRESSOR_OFF);
    Actuator_SetDefrost(DEFROST_OFF);
    Actuator_SetFanSpeed(FAN_OFF);
    Actuator_SetBuzzer(BUZZER_OFF);
    
    // 初始化LED
    for (int i = 0; i < 4; i++) {
        Actuator_SetLED(i, LED_OFF);
    }

    Serial.println("执行器控制初始化完成");
}

// ==================== 压缩机控制 ====================
bool Actuator_CanStartCompressor() {
    // 改为调用 state_machine.cpp 的接口函数
    return StateMachine_CanStartCompressor();
}

bool Actuator_CanStopCompressor() {
    // 改为调用 state_machine.cpp 的接口函数
    return StateMachine_CanStopCompressor();
}

void Actuator_SetCompressor(CompressorState state) {
    // 保护逻辑由 StateMachine 统一管理，此处只执行硬件控制
    g_compressorState = state;
    
    // 控制继电器
    digitalWrite(RELAY_COMPRESSOR_PIN, state == COMPRESSOR_ON ? HIGH : LOW);
    
    // 更新压缩机状态LED
    digitalWrite(LED_COMPRESSOR_PIN, state == COMPRESSOR_ON ? HIGH : LOW);
    
    // 通知 StateMachine 更新保护时间
    StateMachine_ResetCompressorProtection();
    
    Serial.printf("压缩机: %s\n", state == COMPRESSOR_ON ? "启动" : "停止");
}

// ==================== 除霜加热丝控制 ====================
void Actuator_SetDefrost(DefrostState state) {
    g_defrostState = state;
    
    // 控制继电器
    digitalWrite(RELAY_DEFROST_PIN, state == DEFROST_ON ? HIGH : LOW);
    
    Serial.printf("除霜加热丝: %s\n", state == DEFROST_ON ? "开启" : "关闭");
}

// ==================== 风扇控制 ====================
void Actuator_SetFanSpeed(FanSpeed speed) {
    g_fanSpeed = speed;
    ledcWrite(1, speed);
}

void Actuator_SetFanPWM(uint8_t pwmValue) {
    ledcWrite(1, pwmValue);

    if (pwmValue == 0) {
        g_fanSpeed = FAN_OFF;
    } else if (pwmValue < 128) {
        g_fanSpeed = FAN_LOW;
    } else if (pwmValue < 224) {
        g_fanSpeed = FAN_MEDIUM;
    } else {
        g_fanSpeed = FAN_HIGH;
    }
}

// ==================== 蜂鸣器控制 ====================
void Actuator_SetBuzzer(BuzzerState state) {
    g_buzzerState = state;
    
    // 控制蜂鸣器
    digitalWrite(BUZZER_PIN, state == BUZZER_ON ? HIGH : LOW);
    
    Serial.printf("蜂鸣器: %s\n", state == BUZZER_ON ? "开启" : "关闭");
}

void Actuator_Beep(unsigned int durationMs) {
    Actuator_SetBuzzer(BUZZER_ON);
    delay(durationMs);
    Actuator_SetBuzzer(BUZZER_OFF);
}

// ==================== LED控制 ====================
void Actuator_SetLED(int ledId, LEDState state) {
    if (ledId < 0 || ledId > 3) {
        Serial.println("错误: LED ID无效");
        return;
    }
    
    g_ledStates[ledId] = state;
    
    // 根据状态设置LED
    switch (ledId) {
        case 0:  // 运行指示灯(绿)
            if (state == LED_OFF) {
                digitalWrite(LED_RUN_PIN, LOW);
            } else if (state == LED_ON) {
                digitalWrite(LED_RUN_PIN, HIGH);
            }
            // LED_BLINK_SLOW 和 LED_BLINK_FAST 需要在循环中更新
            break;
            
        case 1:  // 待机指示灯(黄)
            if (state == LED_OFF) {
                digitalWrite(LED_STANDBY_PIN, LOW);
            } else if (state == LED_ON) {
                digitalWrite(LED_STANDBY_PIN, HIGH);
            }
            break;
            
        case 2:  // 故障指示灯(红)
            if (state == LED_OFF) {
                digitalWrite(LED_FAULT_PIN, LOW);
            } else if (state == LED_ON) {
                digitalWrite(LED_FAULT_PIN, HIGH);
            }
            break;
            
        case 3:  // 压缩机状态灯(蓝)
            if (state == LED_OFF) {
                digitalWrite(LED_COMPRESSOR_PIN, LOW);
            } else if (state == LED_ON) {
                digitalWrite(LED_COMPRESSOR_PIN, HIGH);
            }
            break;
    }
}

void Actuator_UpdateLEDs() {
    unsigned long currentTime = millis();
    
    for (int i = 0; i < 4; i++) {
        if (g_ledStates[i] == LED_BLINK_SLOW) {
            // 慢闪(500ms)
            if (currentTime - g_ledBlinkTimers[i] > 500) {
                g_ledBlinkTimers[i] = currentTime;
                // 翻转LED状态
                int pin = LED_RUN_PIN;
                if (i == 0) pin = LED_RUN_PIN;
                else if (i == 1) pin = LED_STANDBY_PIN;
                else if (i == 2) pin = LED_FAULT_PIN;
                else if (i == 3) pin = LED_COMPRESSOR_PIN;
                
                digitalWrite(pin, !digitalRead(pin));
            }
        } else if (g_ledStates[i] == LED_BLINK_FAST) {
            // 快闪(200ms)
            if (currentTime - g_ledBlinkTimers[i] > 200) {
                g_ledBlinkTimers[i] = currentTime;
                // 翻转LED状态
                int pin = LED_RUN_PIN;
                if (i == 0) pin = LED_RUN_PIN;
                else if (i == 1) pin = LED_STANDBY_PIN;
                else if (i == 2) pin = LED_FAULT_PIN;
                else if (i == 3) pin = LED_COMPRESSOR_PIN;
                
                digitalWrite(pin, !digitalRead(pin));
            }
        }
    }
}

// ==================== 状态指示 ====================
void Actuator_ShowSystemStatus(int statusCode) {
    // statusCode: 0=运行, 1=待机, 2=故障, 3=压缩机运行
    switch (statusCode) {
        case 0:  // 运行
            Actuator_SetLED(0, LED_ON);       // 运行指示灯亮
            Actuator_SetLED(1, LED_OFF);      // 待机指示灯灭
            Actuator_SetLED(2, LED_OFF);      // 故障指示灯灭
            break;
            
        case 1:  // 待机
            Actuator_SetLED(0, LED_OFF);      // 运行指示灯灭
            Actuator_SetLED(1, LED_ON);       // 待机指示灯亮
            Actuator_SetLED(2, LED_OFF);      // 故障指示灯灭
            break;
            
        case 2:  // 故障
            Actuator_SetLED(0, LED_OFF);      // 运行指示灯灭
            Actuator_SetLED(1, LED_OFF);      // 待机指示灯灭
            Actuator_SetLED(2, LED_ON);       // 故障指示灯亮
            break;
            
        case 3:  // 压缩机运行
            Actuator_SetLED(3, LED_ON);       // 压缩机状态灯亮
            break;
    }
}

// ==================== 报警功能 ====================
void Actuator_Alarm_TemperatureTooHigh(float currentTemp, float setpoint) {
    // 温度过高报警
    if (currentTemp > setpoint + 5.0) {
        Actuator_SetBuzzer(BUZZER_ON);
        Actuator_SetLED(2, LED_BLINK_FAST);  // 故障指示灯快闪
        Serial.println("报警: 温度过高!");
    }
}

void Actuator_Alarm_TemperatureTooLow(float currentTemp, float setpoint) {
    // 温度过低报警
    if (currentTemp < setpoint - 10.0) {
        Actuator_SetBuzzer(BUZZER_ON);
        Actuator_SetLED(2, LED_BLINK_FAST);  // 故障指示灯快闪
        Serial.println("报警: 温度过低!");
    }
}

void Actuator_Alarm_SensorError() {
    // 传感器故障报警
    Actuator_SetBuzzer(BUZZER_ON);
    Actuator_SetLED(2, LED_BLINK_SLOW);  // 故障指示灯慢闪
    Serial.println("报警: 传感器故障!");
}

void Actuator_Alarm_DoorOpen(unsigned long doorOpenTime) {
    // 门未关报警
    if (doorOpenTime > 300000) {  // 5分钟
        Actuator_SetBuzzer(BUZZER_ON);
        Serial.println("报警: 门未关闭!");
    }
}

// ==================== 获取执行器状态 ====================
bool Actuator_IsCompressorOn() {
    return g_compressorState == COMPRESSOR_ON;
}

bool Actuator_IsDefrostOn() {
    return g_defrostState == DEFROST_ON;
}

uint8_t Actuator_GetFanSpeed() {
    return g_fanSpeed;
}

bool Actuator_IsBuzzerOn() {
    return g_buzzerState == BUZZER_ON;
}
