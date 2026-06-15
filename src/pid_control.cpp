/*
 * PID控制 - 单温区PID控制器（冷冻室）
 */
#include "pid_control.h"
#include "sensors_sim.h"
#include "pin_config.h"
#include "actuator.h"

// PID参数 (REVERSE: 制冷系统输出↑→温度↓)
double Kp_freezer = 2.0, Ki_freezer = 5.0, Kd_freezer = 1.0;

double Input_freezer, Output_freezer, Setpoint_freezer = -18.0;

PID myPID_freezer(&Input_freezer, &Output_freezer, &Setpoint_freezer,
                  Kp_freezer, Ki_freezer, Kd_freezer, REVERSE);

double g_freezerSetpoint = -18.0;
SemaphoreHandle_t setpointMutex = NULL;

static unsigned long lastCompressorTime = 0;

void pid_init()
{
  myPID_freezer.SetMode(AUTOMATIC);
  myPID_freezer.SetOutputLimits(0, 255);
  myPID_freezer.SetSampleTime(200);
}

void updatePID()
{
  Input_freezer = g_freezerTemp;
  myPID_freezer.Compute();

  if (Output_freezer > 30.0 && !Actuator_IsCompressorOn())
  {
    Actuator_SetCompressor(COMPRESSOR_ON);
    digitalWrite(COMPRESSOR_LED_PIN, HIGH);
    lastCompressorTime = millis();
    Serial.println("Compressor ON");
  }
  else if (Output_freezer < 10.0 && Actuator_IsCompressorOn())
  {
    Actuator_SetCompressor(COMPRESSOR_OFF);
    digitalWrite(COMPRESSOR_LED_PIN, LOW);
    lastCompressorTime = millis();
    Serial.println("Compressor OFF");
  }
}
