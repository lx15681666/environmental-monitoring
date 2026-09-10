#ifndef APP_FREERTOS_H
#define APP_FREERTOS_H

#include "FreeRTOS.h"
#include "task.h"
#include "Com_debugger.h"

/**
 * @brief 启动FreeRTOS操作系统
 */
void StartFreeRTOS(void);
#endif