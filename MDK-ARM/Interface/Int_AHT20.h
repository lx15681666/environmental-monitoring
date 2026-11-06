#ifndef INT_AHT20_H
#define INT_AHT20_H

#include <string.h>
#include <stdio.h>
#include "i2c.h"
#include "Int_ESP8266.h"
#include <stdlib.h>
#include "cJSON.h"
#include "Com_debugger.h"
#include "Com_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/**
 * 构建records
 */
uint8_t create_record(record_t *record);

/**
 * @brief 生成json数据
 */
char *build_report_data(float temp_value, float humidity_value, uint64_t timetemp);

/**
 * @brief 获取json数据
 * @return json数据指针，失败返回NULL
 */
char *get_json_data(record_t record);

#endif