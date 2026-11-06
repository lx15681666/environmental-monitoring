#ifndef WATCHDOG_ENABLED_H
#define WATCHDOG_ENABLED_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "iwdg.h"
#include "task.h"

/* 任务索引 */
#define TASK_AHT20_IDX 0
#define TASK_CACHE_IDX 1
#define TASK_ESP8266_IDX 2
#define TASK_CMD_IDX 3
#define TASK_NUM 4

/* 各任务心跳，供 watchTask 检查 */
extern volatile uint32_t g_last_beat[TASK_NUM];

void watchTask(); /* 喂狗任务 */

#endif // WATCHDOG_ENABLED_H