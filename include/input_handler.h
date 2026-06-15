/*
 * 输入处理 - 按钮和串口输入
 * 3按钮替代旋转编码器 + 串口键盘模拟
 */
#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <Arduino.h>

void input_init();
void handleEncoder();
void handleSerialInput();

#endif
