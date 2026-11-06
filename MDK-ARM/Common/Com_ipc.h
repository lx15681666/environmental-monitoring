#ifndef COM_IPC_H
#define COM_IPC_H

#include "FreeRTOS.h"
#include "queue.h"
#include "Com_config.h"

/**
 * @brief 创建缓存队列
 */

void IPC_Cache_Init(void);
/**
 * @brief 创建采集数据队列
 */

void IPC_Report_Init(void);

/**
 * @brief 创建队列
 */
uint8_t IPC_SendCache(const record_t rec);

/**
 * @brief record记录数据上报
 */

uint8_t IPC_Report(const record_t record);

/**
 * @brief 接收采集队列数据
 */

uint8_t IPC_ReceiveAht20Data(record_t *record);

/**
 * @brief 接收缓存队列数据
 */
uint8_t IPC_ReceiveAht20DataFromCache(record_t *record);

#endif