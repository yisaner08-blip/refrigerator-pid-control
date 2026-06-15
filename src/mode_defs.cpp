/*
 * 状态机控制库 - 冰箱PID控制系统
 * 实现制冷/除霜/节能/故障四种模式自动切换
 */

#include "state_machine.h"
#include "actuator.h"

// ==================== 内部函数前向声明 ====================
void StateMachine_CoolingMode();
void StateMachine_DefrostMode();
void StateMachine_EcoMode();
void StateMachine_ErrorMode();
void StateMachine_SwitchMode(SystemMode newMode);
void StateMachine_ExitMode(SystemMode mode);
void StateMachine_EnterMode(SystemMode mode);
bool StateMachine_ShouldDefrost();
bool StateMachine_ShouldEnterEco();
void StateMachine_CheckSensorError();
void StateMachine_UpdateLEDs();
void StateMachine_UpdateCompressorProtection();

// ==================== 全局变量 ====================
SystemState g_systemState;
SystemMode g_currentMode = MODE_COOLING;
ControlState g_controlState = STATE_INIT;

// 压缩机保护时间
unsigned long g_compressorOffTime = 0;
unsigned long g_compressorOnTime = 0;
bool g_canStartCompressor = true;
bool g_canStopCompressor = false;

// 除霜控制
unsigned long g_defrostStartTime = 0;
const unsigned long DEFROST_MAX_TIME = 1200000;  // 20分钟 (ms)
bool g_defrostActive = false;

// 节能模式
unsigned long g_doorOpenTime = 0;
const unsigned long ECO_MODE_DELAY = 300000;     // 5分钟 (ms)
bool g_ecoModeActive = false;
bool g_doorEverOpened = false;                   // 是否有人开过门
float g_normalSetpoint = -18.0;

// 故障检测
unsigned long g_lastTempUpdate = 0;
const unsigned long TEMP_TIMEOUT = 30000;  // 30秒无温度更新认为传感器故障
bool g_sensorError = false;

// ==================== 初始化 ====================
void StateMachine_Init() {
    // 初始化系统状态
    g_systemState.currentMode = MODE_COOLING;
    g_systemState.controlState = STATE_INIT;
    g_systemState.freezerTemp = -999.0;
    g_systemState.freshTemp = -999.0;
    g_systemState.setpoint = -18.0;
    g_systemState.doorOpen = false;
    g_systemState.doorOpenTime = 0;
    g_systemState.errorType = ERROR_NONE;
    g_systemState.hasError = false;
    g_systemState.compressorOn = false;
    g_systemState.defrostOn = false;
    g_systemState.fanOn = false;
    g_systemState.stateStartTime = millis();
    g_systemState.lastModeSwitch = millis();
    
    // 初始化保护时间
    g_compressorOffTime = millis();
    g_canStartCompressor = true;
    g_canStopCompressor = false;
    
    // 初始化执行器
    Actuator_Init();
    
    Serial.println("状态机控制初始化完成");
    Serial.println("当前模式: 制冷模式");
}

// ==================== 更新状态机 ====================
void StateMachine_Update(float freezerTemp, float freshTemp, bool doorOpen) {
    // 更新系统状态
    g_systemState.freezerTemp = freezerTemp;
    g_systemState.freshTemp = freshTemp;
    g_systemState.doorOpen = doorOpen;
    
    if (doorOpen) {
        g_systemState.doorOpenTime = millis();
        g_doorEverOpened = true;
    }
    
    // 检查传感器故障
    StateMachine_CheckSensorError();
    
    // 检查门状态
    StateMachine_CheckDoor(doorOpen);
    
    // 根据当前模式执行相应控制
    switch (g_currentMode) {
        case MODE_COOLING:
            StateMachine_CoolingMode();
            break;
        case MODE_DEFROST:
            StateMachine_DefrostMode();
            break;
        case MODE_ECO:
            StateMachine_EcoMode();
            break;
        case MODE_ERROR:
            StateMachine_ErrorMode();
            break;
    }
    
    // 更新执行器
    StateMachine_UpdateActuators();
    
    // 更新LED指示
    StateMachine_UpdateLEDs();
    
    // 更新压缩机保护状态
    StateMachine_UpdateCompressorProtection();
}

// ==================== 制冷模式 ====================
void StateMachine_CoolingMode() {
    // 制冷模式：PID运行，压缩机根据输出启停
    g_systemState.currentMode = MODE_COOLING;
    
    // 检查是否需要除霜
    if (StateMachine_ShouldDefrost()) {
        StateMachine_SwitchMode(MODE_DEFROST);
        return;
    }
    
    // 检查是否进入节能模式
    if (StateMachine_ShouldEnterEco()) {
        StateMachine_SwitchMode(MODE_ECO);
        return;
    }
    
    // 正常制冷控制
    // 压缩机控制由PID输出决定，这里只做模式管理
    g_systemState.defrostOn = false;
    g_systemState.fanOn = true;  // 制冷时风扇运行
    
    // 更新控制状态
    if (g_controlState == STATE_INIT) {
        g_controlState = STATE_STABLE;
    }
}

// ==================== 除霜模式 ====================
void StateMachine_DefrostMode() {
    g_systemState.currentMode = MODE_DEFROST;
    
    if (!g_defrostActive) {
        // 开始除霜
        g_defrostActive = true;
        g_defrostStartTime = millis();
        
        // 关闭压缩机
        g_systemState.compressorOn = false;
        // 打开除霜加热丝
        g_systemState.defrostOn = true;
        // 关闭风扇
        g_systemState.fanOn = false;
        
        Serial.println("开始除霜...");
    }
    
    // 检查除霜是否完成
    bool tempReached = (g_systemState.freezerTemp >= -5.0);  // 温度达到-5°C
    bool timeout = (millis() - g_defrostStartTime > DEFROST_MAX_TIME);
    
    if (tempReached || timeout) {
        // 除霜完成
        g_defrostActive = false;
        g_systemState.defrostOn = false;
        
        Serial.println("除霜完成，返回制冷模式");
        StateMachine_SwitchMode(MODE_COOLING);
    }
}

// ==================== 节能模式 ====================
void StateMachine_EcoMode() {
    g_systemState.currentMode = MODE_ECO;
    
    // 提高设定温度2°C
    float ecoSetpoint = g_normalSetpoint + 2.0;
    g_systemState.setpoint = ecoSetpoint;
    
    // 节能模式下压缩机低频运行
    g_systemState.fanOn = false;  // 关闭风扇节能
    
    // 有人打开门 → 退出节能模式
    if (g_systemState.doorOpen) {
        StateMachine_SwitchMode(MODE_COOLING);
    }
}

// ==================== 故障模式 ====================
void StateMachine_ErrorMode() {
    g_systemState.currentMode = MODE_ERROR;
    g_systemState.hasError = true;
    
    // 关闭所有执行器
    g_systemState.compressorOn = false;
    g_systemState.defrostOn = false;
    g_systemState.fanOn = false;
    
    // 蜂鸣器报警
    Actuator_SetBuzzer(BUZZER_ON);
    
    Serial.printf("系统故障: %s\n", StateMachine_GetErrorName(g_systemState.errorType));
}

// ==================== 模式切换 ====================
void StateMachine_SwitchMode(SystemMode newMode) {
    if (g_currentMode == newMode) return;
    
    Serial.printf("模式切换: %s -> %s\n", 
        StateMachine_GetModeName(g_currentMode),
        StateMachine_GetModeName(newMode));
    
    // 退出当前模式
    StateMachine_ExitMode(g_currentMode);
    
    // 进入新模式
    g_currentMode = newMode;
    g_systemState.lastModeSwitch = millis();
    
    // 进入新模式
    StateMachine_EnterMode(newMode);
}

void StateMachine_ExitMode(SystemMode mode) {
    switch (mode) {
        case MODE_DEFROST:
            g_defrostActive = false;
            g_systemState.defrostOn = false;
            break;
        case MODE_ECO:
            g_systemState.setpoint = g_normalSetpoint;  // 恢复设定温度
            break;
        case MODE_ERROR:
            Actuator_SetBuzzer(BUZZER_OFF);  // 关闭蜂鸣器
            g_systemState.hasError = false;
            break;
        default:
            break;
    }
}

void StateMachine_EnterMode(SystemMode mode) {
    switch (mode) {
        case MODE_COOLING:
            g_systemState.setpoint = g_normalSetpoint;
            g_systemState.fanOn = true;
            break;
        case MODE_DEFROST:
            // 除霜模式初始化在DefrostMode()中处理
            break;
        case MODE_ECO:
            g_systemState.setpoint = g_normalSetpoint + 2.0;
            break;
        case MODE_ERROR:
            // 故障模式初始化
            Actuator_SetBuzzer(BUZZER_ON);
            break;
    }
}

// ==================== 条件检查 ====================
bool StateMachine_ShouldDefrost() {
    // 简化逻辑：每2小时自动除霜一次
    static unsigned long lastDefrostTime = 0;
    unsigned long defrostInterval = 7200000;  // 2小时
    
    if (millis() - lastDefrostTime > defrostInterval) {
        lastDefrostTime = millis();
        return true;
    }
    return false;
}

bool StateMachine_ShouldEnterEco() {
    // 有人开过门 且 门已关闭 且 距上次开门超过5分钟 → 进入节能
    if (!g_systemState.doorOpen && g_doorEverOpened &&
        (millis() - g_systemState.doorOpenTime > ECO_MODE_DELAY)) {
        return true;
    }
    return false;
}

// ==================== 故障检测 ====================
void StateMachine_CheckSensorError() {
    // 检查温度传感器是否断开或读数异常
    if (g_systemState.freezerTemp < -50.0 || g_systemState.freezerTemp > 100.0) {
        if (!g_sensorError) {
            g_sensorError = true;
            g_systemState.errorType = ERROR_SENSOR;
            StateMachine_SwitchMode(MODE_ERROR);
        }
    } else {
        g_sensorError = false;
    }
}

void StateMachine_CheckDoor(bool doorOpen) {
    static unsigned long doorOpenedAt = 0;
    static bool wasDoorOpen = false;

    if (doorOpen && !wasDoorOpen) {
        doorOpenedAt = millis();
    }

    if (doorOpen) {
        if (millis() - doorOpenedAt > 300000) {
            Actuator_SetBuzzer(BUZZER_ON);
            Serial.println("报警: 门未关闭!");
        }
    } else if (wasDoorOpen && !doorOpen) {
        Actuator_SetBuzzer(BUZZER_OFF);
    }

    wasDoorOpen = doorOpen;
}

// ==================== 执行器更新 ====================
void StateMachine_UpdateActuators() {
    // 压缩机继电器（带延时保护）
    if (g_systemState.compressorOn && StateMachine_CanStartCompressor()) {
        Actuator_SetCompressor(COMPRESSOR_ON);
    } else if (!g_systemState.compressorOn && StateMachine_CanStopCompressor()) {
        Actuator_SetCompressor(COMPRESSOR_OFF);
    }

    // 除霜加热丝
    if (g_systemState.defrostOn) {
        Actuator_SetDefrost(DEFROST_ON);
    } else {
        Actuator_SetDefrost(DEFROST_OFF);
    }

    // 风扇速度由 PID PWM 任务控制，此处不干预
}

// ==================== LED更新 ====================
void StateMachine_UpdateLEDs() {
    // 运行指示灯(绿)
    if (g_currentMode == MODE_COOLING || g_currentMode == MODE_ECO) {
        Actuator_SetLED(0, LED_ON);
    } else {
        Actuator_SetLED(0, LED_OFF);
    }
    
    // 待机指示灯(黄) - 节能模式
    if (g_currentMode == MODE_ECO) {
        Actuator_SetLED(1, LED_ON);
    } else {
        Actuator_SetLED(1, LED_OFF);
    }
    
    // 故障指示灯(红)
    if (g_currentMode == MODE_ERROR) {
        Actuator_SetLED(2, LED_BLINK_FAST);
    } else {
        Actuator_SetLED(2, LED_OFF);
    }
    
    // 压缩机状态灯(蓝)
    if (g_systemState.compressorOn) {
        Actuator_SetLED(3, LED_ON);
    } else {
        Actuator_SetLED(3, LED_OFF);
    }
}

// ==================== 压缩机保护 ====================
void StateMachine_UpdateCompressorProtection() {
    unsigned long currentTime = millis();
    
    // 检查是否可以启动（最小停机间隔3分钟）
    if (!g_systemState.compressorOn) {
        unsigned long offTime = currentTime - g_compressorOffTime;
        if (offTime >= MIN_COMPRESSOR_OFF_TIME) {
            g_canStartCompressor = true;
        }
    }
    
    // 检查是否可以停止（最小运行时长5分钟）
    if (g_systemState.compressorOn) {
        unsigned long onTime = currentTime - g_compressorOnTime;
        if (onTime >= MIN_COMPRESSOR_ON_TIME) {
            g_canStopCompressor = true;
        }
    }
}

bool StateMachine_CanStartCompressor() {
    return g_canStartCompressor;
}

bool StateMachine_CanStopCompressor() {
    return g_canStopCompressor;
}

void StateMachine_ResetCompressorProtection() {
    if (g_systemState.compressorOn) {
        g_compressorOnTime = millis();
        g_canStopCompressor = false;
    } else {
        g_compressorOffTime = millis();
        g_canStartCompressor = false;
    }
}

// ==================== 获取状态 ====================
SystemMode StateMachine_GetMode() {
    return g_currentMode;
}

ControlState StateMachine_GetControlState() {
    return g_controlState;
}

SystemState StateMachine_GetState() {
    return g_systemState;
}

// ==================== 名称转换 ====================
const char* StateMachine_GetModeName(SystemMode mode) {
    switch (mode) {
        case MODE_COOLING: return "制冷模式";
        case MODE_DEFROST: return "除霜模式";
        case MODE_ECO:     return "节能模式";
        case MODE_ERROR:   return "故障模式";
        default:            return "未知模式";
    }
}

const char* StateMachine_GetStateName(ControlState state) {
    switch (state) {
        case STATE_INIT:      return "初始化";
        case STATE_STABLE:    return "稳定";
        case STATE_TUNING:   return "调谐中";
        case STATE_RUNNING:   return "运行中";
        default:               return "未知状态";
    }
}

const char* StateMachine_GetErrorName(ErrorType error) {
    switch (error) {
        case ERROR_NONE:      return "无故障";
        case ERROR_SENSOR:    return "传感器故障";
        case ERROR_TEMP_HIGH: return "温度过高";
        case ERROR_TEMP_LOW:  return "温度过低";
        case ERROR_DOOR_OPEN: return "门未关闭";
        case ERROR_COMPRESSOR: return "压缩机故障";
        default:              return "未知故障";
    }
}

// ==================== 设置功能 ====================
void StateMachine_SetMode(SystemMode mode) {
    if (mode >= MODE_COOLING && mode <= MODE_ERROR) {
        StateMachine_SwitchMode(mode);
    }
}

void StateMachine_SetTargetTemp(float temp) {
    g_normalSetpoint = temp;
    g_systemState.setpoint = temp;
    
    if (g_currentMode == MODE_ECO) {
        g_systemState.setpoint = temp + 2.0;
    }
}

float StateMachine_GetTargetTemp() {
    return g_normalSetpoint;
}

void StateMachine_ClearError() {
    if (g_currentMode == MODE_ERROR) {
        g_systemState.hasError = false;
        g_systemState.errorType = ERROR_NONE;
        Actuator_SetBuzzer(BUZZER_OFF);
        StateMachine_SwitchMode(MODE_COOLING);
        Serial.println("故障已清除，返回制冷模式");
    }
}
