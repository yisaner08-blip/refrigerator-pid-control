/*
 * 温度历史记录 - 环形缓冲区
 * 温度历史用环形缓冲区实现，HISTORY_SIZE=60，每秒存一条，满了自动覆盖。
 * Web 端和小程序通过 /api/history 接口读取 JSON 格式的历史数据。
 */
#include "temp_history.h"
#include <ArduinoJson.h>

static float tempHistory[HISTORY_SIZE];//温度数据存储数组，60 个 float，存历史温度值
static int historyIndex = 0;//写指针，指向下一个要写入的位置
static bool historyFilled = false;//缓冲区满标志
static unsigned long lastTempRecord = 0;//上次记录时间

void temp_history_record(float temp)
{
  if (millis() - lastTempRecord > 1000)//判断是否距离上次记录时间超过1秒，超过 1 秒才允许写入
  {
    tempHistory[historyIndex] = temp;//把温度存到缓冲区
    historyIndex = (historyIndex + 1) % HISTORY_SIZE;//写指针后移
    if (historyIndex == 0) historyFilled = true;//满圈检测
    lastTempRecord = millis();//重置计时
  }
}

String getTempHistoryJSON()
{
  DynamicJsonDocument doc(1024);//分配 1024 字节内存，够存 60 条浮点数
  JsonArray freezer = doc.createNestedArray("freezer");//创建嵌套数组

  int startIdx = historyFilled ? historyIndex : 0;//起始索引，如果缓冲区满了，则从写指针开始读，否则从 0 开始读
  int count = historyFilled ? HISTORY_SIZE : historyIndex;//读取数量，如果缓冲区满了，则读取 HISTORY_SIZE 条，否则读取写指针位置的条数

  for (int i = 0; i < count; i++)//读取 count 条数据，从 startIdx 开始
  {
    int idx = (startIdx + i) % HISTORY_SIZE;//读出所有有效数据
    freezer.add(tempHistory[idx]);//计算实际数组下标
  }

  String response;
  serializeJson(doc, response);//将 JSON 序列化为字符串
  return response;//返回 JSON 字符串
}
