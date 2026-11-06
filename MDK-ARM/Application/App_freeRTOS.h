#ifndef APP_FREERTOS_H
#define APP_FREERTOS_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "Com_debugger.h"
#include "Com_ipc.h"
#include "Int_Flash.h"
#include "Int_AHT20.h"
#include "Int_ESP8266.h"
#include "limits.h"
#include "Int_led.h"
#include "gpio.h"
#include "App_watchdog.h"
#include "App_OTA.h"
/**
 * @brief 启动FreeRTOS操作系统
 */
void StartFreeRTOS(void);
#endif