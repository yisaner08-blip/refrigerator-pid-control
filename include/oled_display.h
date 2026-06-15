/*
 * OLED显示 - SSD1306 128x64显示屏
 * 主界面和菜单界面的绘制
 */
#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <Arduino.h>
#include <U8g2lib.h>

// OLED对象
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// 菜单状态
extern bool inMenu;
extern int menuSelection;

void oled_init();
void updateOLED();

#endif
