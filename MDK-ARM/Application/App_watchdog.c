#include "App_watchdog.h"

/* 心跳数组定义（只在这一个文件定义） */
volatile uint32_t g_last_beat[TASK_NUM] = {0};

/* 各任务超时阈值（必须 > 心跳刷新周期） */
static const uint32_t task_timeout_ms[TASK_NUM] = {
    1000, /* AHT20   */
    1000, /* cache   */
    5000, /* ESP8266 */
    1000, /* cmd     */
};
#define WATCH_PERIOD 200

void watchTask() /* 喂狗任务 */
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (;;)
    {
        uint32_t now = xTaskGetTickCount();
        int healthy = 1;
        for (int i = 0; i < TASK_NUM; i++)
        {
            if (now - g_last_beat[i] > task_timeout_ms[i])
            {
                healthy = 0;
                break;
            }
        }
        if (healthy)

            HAL_IWDG_Refresh(&hiwdg);
        /* 卡死则不再喂狗 → IWDG 复位 */
        vTaskDelayUntil(&xLastWakeTime, WATCH_PERIOD);
    }
}