/*
 * 输入处理 - 按钮和串口输入
 */
#include "input_handler.h"
#include "pin_config.h"
#include "oled_display.h"
#include "pid_control.h"
#include "system_control.h"
#include "actuator.h"
#include "sensors_sim.h"

static bool btnAWasPressed = false;
static bool btnBWasPressed = false;
static bool btnSWWasPressed = false;
static int btnA_REST = HIGH;
static int btnB_REST = HIGH;
static int btnSW_REST = HIGH;

void input_init()
{
  pinMode(ENC_PIN_A, INPUT_PULLUP);
  pinMode(ENC_PIN_B, INPUT_PULLUP);
  pinMode(ENC_PIN_SW, INPUT_PULLUP);
  delay(50);

  btnA_REST = digitalRead(ENC_PIN_A);
  btnB_REST = digitalRead(ENC_PIN_B);
  btnSW_REST = digitalRead(ENC_PIN_SW);
  Serial.printf("Btn rest states: A=%d B=%d SW=%d\n", btnA_REST, btnB_REST, btnSW_REST);
}

void handleEncoder()
{
  bool btnA = (digitalRead(ENC_PIN_A) != btnA_REST);
  bool btnB = (digitalRead(ENC_PIN_B) != btnB_REST);
  bool btnSW = (digitalRead(ENC_PIN_SW) != btnSW_REST);

  // A 按钮：上/左
  if (btnA && !btnAWasPressed)
  {
    btnAWasPressed = true;
    if (inMenu)
      menuSelection = (menuSelection > 0) ? menuSelection - 1 : 3;
  }
  if (!btnA) btnAWasPressed = false;

  // B 按钮：下/右
  if (btnB && !btnBWasPressed)
  {
    btnBWasPressed = true;
    if (inMenu)
      menuSelection = (menuSelection + 1) % 4;
  }
  if (!btnB) btnBWasPressed = false;

  // SW 按钮：确认/菜单
  if (btnSW && !btnSWWasPressed)
  {
    btnSWWasPressed = true;
    Serial.println("[BTN] SW pressed");
    if (inMenu)
    {
      if (menuSelection == 0)
      {
        currentMode = (SystemMode)((currentMode + 1) % 4);
        Serial.printf("[BTN] mode -> %d\n", currentMode);
      }
      else if (menuSelection == 1)
      {
        Setpoint_freezer += 0.5;
        if (Setpoint_freezer > 10.0) Setpoint_freezer = -30.0;
        g_freezerSetpoint = Setpoint_freezer;
        Serial.printf("[BTN] setpoint -> %.1f\n", Setpoint_freezer);
      }
      else if (menuSelection == 2)
      {
        inMenu = false;
        menuSelection = 0;
        Serial.println("[BTN] back to main");
      }
      else
      {
        resetSystem();
        Serial.println("[BTN] System Full Reset (Setpoint to -18C)");
      }
    }
    else
    {
      inMenu = true;
      menuSelection = 0;
      Serial.println("[BTN] entered menu");
    }
  }
  if (!btnSW) btnSWWasPressed = false;
}

void handleSerialInput()
{
  if (Serial.available() > 0)
  {
    char cmd = Serial.read();

    if (cmd == 'a' || cmd == 'A')
    {
      if (inMenu)
      {
        menuSelection = (menuSelection > 0) ? menuSelection - 1 : 3;
        Serial.println("[Serial Input] Button A pressed");
      }
    }
    else if (cmd == 'b' || cmd == 'B')
    {
      if (inMenu)
      {
        menuSelection = (menuSelection + 1) % 4;
        Serial.println("[Serial Input] Button B pressed");
      }
    }
    else if (cmd == 't' || cmd == 'T')
    {
      g_useSimulatedTemp = !g_useSimulatedTemp;
      Serial.printf("[Serial] Temperature source: %s\n",
                    g_useSimulatedTemp ? "SIMULATED" : "NTC SENSOR");
    }
    {
      Serial.println("[Serial Input] SW pressed");
      if (inMenu)
      {
        if (menuSelection == 0)
        {
          currentMode = (SystemMode)((currentMode + 1) % 4);
          Serial.printf("[Serial] mode -> %d\n", currentMode);
        }
        else if (menuSelection == 1)
        {
          Setpoint_freezer += 0.5;
          if (Setpoint_freezer > 10.0) Setpoint_freezer = -30.0;
          g_freezerSetpoint = Setpoint_freezer;
          Serial.printf("[Serial] setpoint -> %.1f\n", Setpoint_freezer);
        }
        else if (menuSelection == 2)
        {
          inMenu = false;
          menuSelection = 0;
          Serial.println("[Serial] back to main");
        }
        else
        {
          resetSystem();
          Serial.println("[Serial] System Full Reset (Setpoint to -18C)");
        }
      }
      else
      {
        inMenu = true;
        menuSelection = 0;
        Serial.println("[Serial] entered menu");
      }
    }
  }
}
