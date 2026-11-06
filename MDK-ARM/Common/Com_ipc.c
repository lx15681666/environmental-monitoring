#include "Com_ipc.h"

static QueueHandle_t xCacheQueue = NULL;  // 离线缓存队列
static QueueHandle_t xReportQueue = NULL; // 上报队列（在线时采集 → ESP8266）

/**
 * @brief 创建缓存队列
 */

void IPC_Cache_Init(void)
{
    xCacheQueue = xQueueCreate(16, sizeof(record_t));
}
/**
 * @brief 创建采集数据队列
 */

void IPC_Report_Init(void)
{
    xReportQueue = xQueueCreate(16, sizeof(record_t));
}

/**
 * @brief 创建缓存队列
 */
uint8_t IPC_SendCache(const record_t rec)
{
    if (xCacheQueue == NULL)
        return 0;
    return xQueueSend(xCacheQueue, &rec, pdMS_TO_TICKS(100));
}

/**
 * @brief record记录数据上报
 */

uint8_t IPC_Report(const record_t record)
{
    if (xReportQueue == NULL)
        return 0;
    return xQueueSend(xReportQueue, &record, pdMS_TO_TICKS(100));
}

/**
 * @brief 接收采集队列数据
 */

uint8_t IPC_ReceiveAht20Data(record_t *record)
{
    if (xReportQueue == NULL || record == NULL)
        return 0;
    return xQueueReceive(xReportQueue, record, 0);
}

/**
 * @brief 接收缓存队列数据
 */
uint8_t IPC_ReceiveAht20DataFromCache(record_t *record)
{
    if (xCacheQueue == NULL || record == NULL)
        return 0;
    return xQueueReceive(xCacheQueue, record, 0);
}