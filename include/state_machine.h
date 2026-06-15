/*
 * 状态机控制库 - 冰箱PID控制系统
 * 实现制冷/除霜/节能/故障四种模式自动切换
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <Arduino.h>

// ==================== 系统模式枚举 ====================
typedef enum {
    MODE_COOLING = 0,    // 制冷模式
    MODE_DEFROST = 1,     // 除霜模式
    MODE_ECO = 2,          // 节能模式
    MODE_ERROR = 3         // 故障模式
} SystemMode;

// ==================== 控制状态枚举 ====================
typedef enum {
    STATE_INIT = 0,           // 初始化状态
    STATE_STABLE = 1,         // 温度稳定状态
    STATE_TUNING = 2,         // PID自动调谐状态
    STATE_RUNNING = 3          // 正常运行状态
} ControlState;

// ==================== 故障类型枚举 ====================
typedef enum {
    ERROR_NONE = 0,          // 无故障
    ERROR_SENSOR = 1,        // 传感器故障
    ERROR_TEMP_HIGH = 2,      // 温度过高
    ERROR_TEMP_LOW = 3,        // 温度过低
    ERROR_DOOR_OPEN = 4,      // 门未关闭
    ERROR_COMPRESSOR = 5       // 压缩机故障
} ErrorType;

// ==================== 系统状态数据结构 ====================
typedef struct {
    // 当前模式
    SystemMode currentMode;
    ControlState controlState;
    
    // 温度数据
    float freezerTemp;           // 冷冻室温度
    float freshTemp;             // 冷藏室温度
    float setpoint;              // 设定温度
    
    // 门状态
    bool doorOpen;              // 门是否打开
    unsigned long doorOpenTime;   // 门打开时间
    
    // 故障状态
    ErrorType errorType;         // 故障类型
    bool hasError;              // 是否有故障
    
    // 执行器状态
    bool compressorOn;          // 压缩机是否运行
    bool defrostOn;            // 除霜加热丝是否开启
    bool fanOn;                 // 风扇是否运行
    
    // 时间戳
    unsigned long stateStartTime; // 当前状态开始时间
    unsigned long lastModeSwitch; // 上次模式切换时间
} SystemState;

// ==================== 函数声明 ====================

// 初始化状态机
void StateMachine_Init();

// 更新状态机 (需在主循环中定期调用)
void StateMachine_Update(float freezerTemp, float freshTemp, bool doorOpen = false);

// 获取当前系统模式
SystemMode StateMachine_GetMode();

// 获取当前控制状态
ControlState StateMachine_GetControlState();

// 获取当前系统状态
SystemState StateMachine_GetState();

// 设置系统模式 (手动切换模式)
void StateMachine_SetMode(SystemMode mode);

// 获取模式名称 (用于显示)
const char* StateMachine_GetModeName(SystemMode mode);

// 获取状态名称 (用于显示)
const char* StateMachine_GetStateName(ControlState state);

// 获取故障类型名称 (用于显示)
const char* StateMachine_GetErrorName(ErrorType error);

// 检查是否可以启动压缩机 (压缩机延时保护)
bool StateMachine_CanStartCompressor();

// 检查是否可以停止压缩机 (最小运行时间保护)
bool StateMachine_CanStopCompressor();

// 重置压缩机保护时间
void StateMachine_ResetCompressorProtection();

// 清除故障
void StateMachine_ClearError();

// 检查门状态 (门未关报警)
void StateMachine_CheckDoor(bool doorOpen);

// 更新执行器状态 (根据当前模式和PID输出)
void StateMachine_UpdateActuators();

// 获取目标温度 (根据当前模式自动调整)
float StateMachine_GetTargetTemp();

// 设置目标温度
void StateMachine_SetTargetTemp(float temp);

#endif // STATE_MACHINE_H_
