/*
 * 传感器 - Wokwi仿真版本
 * 支持软件模拟温度和真实NTC传感器两种模式
 */
#ifndef SENSORS_SIM_H
#define SENSORS_SIM_H

#include <Arduino.h>

// 温度数据源切换（默认使用模拟温度）
extern bool g_useSimulatedTemp;
// 温度数据源选择：true=使用软件模拟温度，false=读取真实NTC传感器
// （Wokwi仿真时可用滑动变阻器模拟NTC输入，或直接使用软件模拟温度）

// 模拟温度（软件模拟时使用）
extern float simulatedTemp;

// Web服务器需要的全局温度变量（两种模式共用）
extern float g_freezerTemp;

// 函数声明
float readNTCTemperature(int pin);//pin：NTC 热敏电阻连接的 ESP32 ADC 引脚编号
//统一接口，根据 g_useSimulatedTemp 开关选择数据源
//调用位置：未被调用。原因：后来改为直接用全局变量 g_freezerTemp，简化数据流

float readNTCRealTemperature();
//读取NTC传感器温度（ADC + Beta公式 + EMA滤波）

float readSHT40Temperature();
//作用：模拟SHT40温度（随机数），调用位置：未被调用
//调用位置：sensors_sim.cpp:35 ← 被 readNTCTemperature() 调用。sensors_sim.cpp:78 ← 被 simulateTemperature() 在真实模式下调用

float readSHT40Humidity();
//作用：模拟SHT40湿度（随机数），调用位置：未被调用

void simulateTemperature();
//作用：根据系统状态模拟温度变化，更新 g_freezerTemp
//调用位置：wokwi_main.cpp:118 ← 在主循环中每秒调用

#endif
