#include "App_OTA.h"

/**是否正在下载标志位 */
volatile uint8_t ota_downloading;

#define OTA_PERIOD 300

void download_task(void)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    for (;;)
    {
        OTAJSON_t ota_json_detail = get_ota_json();
        if (ota_json_detail.isOTA == 1)
        { // 确认是否需要OTA升级
            // debugger_printf("data is%d, %s", ota_json_detail.isOTA, ota_json_detail.md5);
        }
        vTaskDelayUntil(&xLastWakeTime, OTA_PERIOD);
    }
}