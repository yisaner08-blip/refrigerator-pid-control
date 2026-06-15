/*
 * 传感器 - Wokwi仿真版本
 * 支持两种模式：软件模拟温度（默认）和真实NTC传感器
 */
#include "sensors_sim.h"
#include "temp_history.h"
#include "actuator.h"
#include "system_control.h"
#include "pin_config.h"

// 默认使用软件模拟温度，可通过串口命令 't' 切换
bool g_useSimulatedTemp = true;

// 模拟温度
float simulatedTemp = -18.0;

// Web服务器需要的全局温度变量
float g_freezerTemp = -18.0;

// 温度物理模拟计时
static unsigned long lastTempChange = 0;

// NTC读数EMA滤波
static float g_filteredNTCTemp = -999;
static bool g_firstNTCReading = true;

float readNTCTemperature(int pin)
{
  if (g_useSimulatedTemp)
  {
    return simulatedTemp;
  }
  else
  {
    return readNTCRealTemperature();
  }
}

float readNTCRealTemperature()//真实传感器读取，含Beta公式+EMA滤波
{
  int adc = analogRead(NTC_FREEZER_PIN);//读取ADC值，限制范围：0-4095
  if (adc <= 1)
    adc = 1;
  if (adc >= 4094)
    adc = 4094;

  // Beta公式（Wokwi标准公式，R_fixed=R0隐含在分压比中）
  float tempC = 1.0 / (log(1.0 / (4095.0 / adc - 1.0)) / 3950.0 + 1.0 / 298.15) - 273.15;

  // EMA滤波平滑
  if (g_firstNTCReading)
  {
    g_filteredNTCTemp = tempC;//第一次读数，直接赋值
    g_firstNTCReading = false;
  }
  else
  {
    g_filteredNTCTemp = g_filteredNTCTemp * 0.7 + tempC * 0.3;//EMA滤波
    //新值 = 旧值 * α + 当前值 * (1-α)
    //这里 α = 0.7，即 新值 = 旧值*0.7 + 当前值*0.3
  }

  return g_filteredNTCTemp;
}

float readSHT40Temperature()//模拟SHT40，用随机数
{
  return 25.0 + random(-20, 20) / 10.0;
}

float readSHT40Humidity()//模拟SHT40，用随机数
{
  return 60.0 + random(-100, 100) / 10.0;
}

void simulateTemperature()//核心，控制温度模拟逻辑
{
  if (!g_useSimulatedTemp)//如果不是模拟温度，则直接读取真实传感器，g_useSimulatedTemp=true 表示用模拟温度
  {
    g_freezerTemp = readNTCRealTemperature();//读取真实传感器温度
    temp_history_record(g_freezerTemp);//记录温度历史
    return;//直接返回，不进行模拟
  }

  if (millis() - lastTempChange > 1000)//每秒更新一次模拟温度
  {
    if (currentMode == MODE_DEFROST && Actuator_IsDefrostOn())//除霜模式，会加热灯丝，使温度上升
    {
      simulatedTemp += 0.8;//让温度每秒上升0.8°
    }
    else if (currentMode == MODE_ERROR)//故障模式，此时压缩机不工作，温度会缓慢上升，但小于除霜模式
    {
      simulatedTemp += 0.1;//让温度每秒上升0.1°
    }
    else if (Actuator_IsCompressorOn())//检查压缩机状态：如果正在工作，模拟制冷效果（温度下降）。Actuator_IsCompressorOn()：压缩机工作时候会返回true
    {
      simulatedTemp -= 0.5;//让温度每秒下降0.5°
    }
    else//待机模式，温度缓慢上升，状态和故障模式相同，只是这个是主动，故障模式是被动
    {
      simulatedTemp += 0.1;//让温度每秒上升0.1°
    }

    if (simulatedTemp < -30.0)
      simulatedTemp = -30.0;
    if (simulatedTemp > 10.0)
      simulatedTemp = 10.0;
    /*存在问题：当预设温度是-30时候，制冷到-30后，压缩机依然会工作
    解决方案：重置积分项（当温度达到限幅值时）；强制停止压缩机（当温度达到限幅值时）；改进限幅逻辑（让限幅值更极端，不影响PID计算）
    */

    lastTempChange = millis();

    g_freezerTemp = simulatedTemp;//把模拟计算出的温度，写入全局变量 g_freezerTemp
  }

  temp_history_record(simulatedTemp); // 存入环形缓冲区，供网页端和微信小程序查询温度历史
}
