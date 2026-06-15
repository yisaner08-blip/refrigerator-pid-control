/*
 * 系统控制 - 模式管理、执行器控制、安全检查和系统复位
 */
#ifndef SYSTEM_CONTROL_H
#define SYSTEM_CONTROL_H

#include <Arduino.h>
#include "state_machine.h"

// 当前系统模式（所有模块共享）
extern SystemMode currentMode;

void controlActuators();
void checkSafety();
void updateSystemMode();
void resetSystem();

#endif
