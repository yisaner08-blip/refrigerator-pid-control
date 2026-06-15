/*
 * OLED显示 - SSD1306 128x64显示屏
 */
#include "oled_display.h"
#include "pin_config.h"
#include "sensors_sim.h"
#include "pid_control.h"
#include "system_control.h"
#include "actuator.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

bool inMenu = false;
int menuSelection = 0;

void oled_init()
{
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setCursor(0, 10);
  u8g2.print("Initializing...");
  u8g2.sendBuffer();
}

void updateOLED()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  if (!inMenu)
  {
    u8g2.setCursor(0, 10);
    u8g2.print("Fridge PID Control");

    u8g2.setCursor(0, 22);
    u8g2.print("Freezer: ");
    u8g2.print(g_freezerTemp, 1);
    u8g2.print(" C");

    u8g2.setCursor(0, 34);
    u8g2.print("Target:  ");
    u8g2.print(g_freezerSetpoint, 1);
    u8g2.print(" C");

    u8g2.setCursor(0, 46);
    u8g2.print("Comp:");
    u8g2.print(Actuator_IsCompressorOn() ? " ON " : "OFF");
    u8g2.print("  PID:");
    u8g2.print(Output_freezer, 0);

    u8g2.setCursor(0, 58);
    u8g2.print("Mode: ");
    switch (currentMode)
    {
    case MODE_COOLING: u8g2.print("COOLING"); break;
    case MODE_DEFROST: u8g2.print("DEFROST"); break;
    case MODE_ECO:     u8g2.print("ECO");     break;
    case MODE_ERROR:   u8g2.print("ERROR");   break;
    }
  }
  else
  {
    u8g2.setCursor(0, 10);
    u8g2.print("Menu");
    u8g2.drawFrame(0, 15, 128, 45);

    u8g2.setCursor(5, 22);
    if (menuSelection == 0) u8g2.print("> ");
    u8g2.print("Mode: ");
    switch (currentMode)
    {
    case MODE_COOLING: u8g2.print("Cooling"); break;
    case MODE_DEFROST: u8g2.print("Defrost"); break;
    case MODE_ECO:     u8g2.print("Eco");     break;
    case MODE_ERROR:   u8g2.print("Error");   break;
    }

    u8g2.setCursor(5, 34);
    if (menuSelection == 1) u8g2.print("> ");
    u8g2.print("Set Temp: ");
    u8g2.print(g_freezerSetpoint, 1);

    u8g2.setCursor(5, 46);
    if (menuSelection == 2) u8g2.print("> ");
    u8g2.print("Back");

    u8g2.setCursor(5, 58);
    if (menuSelection == 3) u8g2.print("> ");
    u8g2.print("Reset");
  }

  u8g2.sendBuffer();
}
