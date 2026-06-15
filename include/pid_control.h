/*
 * PID控制 - 单温区PID控制器（冷冻室）
 */
#ifndef PID_CONTROL_H
#define PID_CONTROL_H

#include <Arduino.h>
#include <PID_v1.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// PID参数
extern double Kp_freezer, Ki_freezer, Kd_freezer;

// PID输入输出变量
extern double Input_freezer, Output_freezer, Setpoint_freezer;

// PID对象
extern PID myPID_freezer;

// Web共享的温度设定值
extern double g_freezerSetpoint;

// Web请求同步互斥锁
extern SemaphoreHandle_t setpointMutex;

void pid_init();
void updatePID();

#endif
