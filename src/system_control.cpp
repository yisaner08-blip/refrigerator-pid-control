/*
 * 系统控制 - 模式管理、执行器控制、安全检查和系统复位
 */
#include "system_control.h"
#include "pin_config.h"
#include "sensors_sim.h"
#include "pid_control.h"
#include "oled_display.h"
#include "actuator.h"

SystemMode currentMode = MODE_COOLING;

static unsigned long lastDefrostTime = 0;

void controlActuators()
{
  digitalWrite(COMPRESSOR_LED_PIN, Actuator_IsCompressorOn() ? HIGH : LOW);

  if (currentMode == MODE_DEFROST && !Actuator_IsDefrostOn())
  {
    Actuator_SetDefrost(DEFROST_ON);
    digitalWrite(DEFROST_LED_PIN, HIGH);
    lastDefrostTime = millis();
    Serial.println("Defrost ON");
  }
  else if (currentMode != MODE_DEFROST && Actuator_IsDefrostOn())
  {
    Actuator_SetDefrost(DEFROST_OFF);
    digitalWrite(DEFROST_LED_PIN, LOW);
    Serial.println("Defrost OFF");
  }

  if (currentMode == MODE_ERROR)
  {
    Actuator_SetBuzzer(BUZZER_ON);
    digitalWrite(BUZZER_PIN, (millis() / 500) % 2);
  }
  else
  {
    Actuator_SetBuzzer(BUZZER_OFF);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void checkSafety()
{
  if (isnan(g_freezerTemp) || g_freezerTemp < -50.0 || g_freezerTemp > 50.0)
  {
    currentMode = MODE_ERROR;
    Serial.println("ERROR: Temperature sensor fault!");
  }

  if (g_freezerTemp > 10.0)
  {
    currentMode = MODE_ERROR;
    Serial.println("ERROR: Temperature too high!");
  }

  if (g_freezerTemp < -30.0)
  {
    currentMode = MODE_ERROR;
    Serial.println("ERROR: Temperature too low!");
  }
}

void updateSystemMode()
{
  switch (currentMode)
  {
  case MODE_COOLING:
    break;
  case MODE_DEFROST:
    Actuator_SetCompressor(COMPRESSOR_OFF);
    digitalWrite(COMPRESSOR_LED_PIN, LOW);
    if (millis() - lastDefrostTime > 120000)
    {
      currentMode = MODE_COOLING;
      Serial.println("Defrost completed");
    }
    break;
  case MODE_ECO:
    Setpoint_freezer = -16.0;
    g_freezerSetpoint = Setpoint_freezer;
    break;
  case MODE_ERROR:
    Actuator_SetCompressor(COMPRESSOR_OFF);
    Actuator_SetDefrost(DEFROST_OFF);
    digitalWrite(COMPRESSOR_LED_PIN, LOW);
    digitalWrite(DEFROST_LED_PIN, LOW);
    break;
  }
}

void resetSystem()
{
  currentMode = MODE_COOLING;
  Setpoint_freezer = -18.0;
  g_freezerSetpoint = -18.0;
  g_freezerTemp = -18.0;
  simulatedTemp = -18.0;

  Actuator_SetCompressor(COMPRESSOR_OFF);
  Actuator_SetDefrost(DEFROST_OFF);
  Actuator_SetBuzzer(BUZZER_OFF);
  digitalWrite(COMPRESSOR_LED_PIN, LOW);
  digitalWrite(DEFROST_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  myPID_freezer.SetMode(MANUAL);
  Output_freezer = 0;
  myPID_freezer.SetMode(AUTOMATIC);

  inMenu = false;
  menuSelection = 0;
}
