/* 
 * 集中管理所有硬件引脚的宏定义
 * 引脚定义 - Wokwi仿真版本
 */
#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

// OLED I2C
#define OLED_SDA 8
#define OLED_SCL 9

// NTC温度传感器 (ESP32 ADC)，冷冻室/冷藏室温度传感器
#define NTC_FREEZER_PIN 1
#define NTC_FRESH_PIN 2

// 按钮（3按钮替代编码器）
#define ENC_PIN_A 4
#define ENC_PIN_B 5
#define ENC_PIN_SW 6

// Wokwi仿真指示灯
#define COMPRESSOR_LED_PIN 18
#define DEFROST_LED_PIN 19

#endif
