/*
 * 温度历史记录 - 环形缓冲区
 * 提供温度历史数据的存储和JSON导出
 */
#ifndef TEMP_HISTORY_H
#define TEMP_HISTORY_H

#include <Arduino.h>

#define HISTORY_SIZE 60//存最近 60 条温度记录，只保留最近 1 分钟的温度历史

// 每秒记录一次温度到环形缓冲区
// 数据通过 /api/history 接口提供给网页端和微信小程序
void temp_history_record(float temp);

//把环形缓冲区里的温度历史序列化成 JSON，供 /api/history 接口返回给前端。
String getTempHistoryJSON();
//返回的 JSON 格式：{"freezer": [-18.5, -19.0, -19.2, ...]}

#endif
