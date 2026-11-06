#include "App_freeRTOS.h"
#include "semphr.h"

LED_t control_led = {.port = control_led_GPIO_Port, .pin = control_led_Pin}; // 定义led1结构体，指定端口和引脚

#define CACHE_QUEUE_SIZE 16

void RunESP8266Task();
#define RunESP8266_STACK_SIZE 1024 // 一般最小是128个字，一个字等于4字节
#define RunESP8266_PRIORITY 2
TaskHandle_t RunESP8266_Handle;
#define ESP8266_TASK_PERIOD 50

void RunAHT20Task();
#define AHT20Task_STACK_SIZE 512 // 一般最小是128个字，一个字等于4字节
#define AHT20Task_PRIORITY 3
TaskHandle_t AHT20Task_Handle;
#define AHT20_TASK_PERIOD 100

void Listen_Control_Task();
#define Listen_Control_Task_STACK_SIZE 512 // 一般最小是128个字，一个字等于4字节
#define Listen_Control_Task_PRIORITY 1
TaskHandle_t Listen_Control_Task_Handle;
/**
 * ota升级任务
 */
#define OTA_TASK_SIZE 128 // 一般最小是128个字，一个字等于4字节
#define OTA_PRIORITY 2
TaskHandle_t OTA_Handle;

void cacheTask();            // 离线缓存任务
#define cache_STACK_SIZE 128 // 一般最小是128个字，一个字等于4字节
#define cache_PRIORITY 2
TaskHandle_t cache_Handle;
#define CACHE_PERIOD 79

#define watch_STACK_SIZE 128 // 一般最小是128个字，一个字等于4字节
#define watch_PRIORITY 4
TaskHandle_t watch_Handle;

// void resendTask();            // 重发任务
// #define resend_STACK_SIZE 512 // 一般最小是128个字，一个字等于4字节
// #define resend_PRIORITY 2
// TaskHandle_t resend_Handle;
// #define reSend_PERIOD 2000

void Debug_Task();           // 任务监控任务
#define Debug_STACK_SIZE 128 // 一般最小是128个字，一个字等于4字节
#define Debug_PRIORITY 3
TaskHandle_t Debug_Handle;
#define Debug_PERIOD 2000

// SemaphoreHandle_t xSemaphore;
void StartFreeRTOS()
{
    debugger_test();
    IPC_Report_Init(); // 初始化采集数据队列
    IPC_Cache_Init();  // 初始化缓存队列
    // xSemaphore = xSemaphoreCreateBinary(); // 创建二值信号量
    cacl_recover_cache_ctl(); // 获取缓存控制块
    uint8_t canErase = is_sctor11_full_check();
    if (canErase == 0)
        flash_erase_sector(); // 擦除
    // 数据采集任务
    xTaskCreate(RunAHT20Task, "AHT20Task", AHT20Task_STACK_SIZE, NULL, AHT20Task_PRIORITY, &AHT20Task_Handle);

    // 离线缓存任务
    xTaskCreate(cacheTask, "cacheTask", cache_STACK_SIZE, NULL, cache_PRIORITY, &cache_Handle);

    // 重发任务
    // xTaskCreate(resendTask, "resendTask", resend_STACK_SIZE, NULL, resend_PRIORITY, &resend_Handle);

    // 互联网任务
    xTaskCreate(RunESP8266Task, "RunESP8266Task", RunESP8266_STACK_SIZE, NULL, RunESP8266_PRIORITY, &RunESP8266_Handle);

    // 监听云端命令控制led任务
    xTaskCreate(Listen_Control_Task, "Listen_Control_Task", Listen_Control_Task_STACK_SIZE, NULL, Listen_Control_Task_PRIORITY, &Listen_Control_Task_Handle);

    // debug任务
    // xTaskCreate(Debug_Task, "Debug_Task", Debug_STACK_SIZE, NULL, Debug_PRIORITY, &Debug_Handle);

    // 喂狗任务
    xTaskCreate(watchTask, "watchTask", watch_STACK_SIZE, NULL, watch_PRIORITY, &watch_Handle);

    // ota升级任务
    xTaskCreate(download_task, "download_task", OTA_TASK_SIZE, NULL, OTA_PRIORITY, &OTA_Handle);

    // 启用任务调度
    vTaskStartScheduler();
}

// 监听云端命令任务
void Listen_Control_Task()
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1)
    {
        uint8_t value = get_net_cmd(); // 获取网络命令
        if (value == 0)
        {
            led_on(&control_led);
        }
        else if (value == 1)
        {
            led_off(&control_led);
        }
        g_last_beat[TASK_CMD_IDX] = xTaskGetTickCount();
        vTaskDelayUntil(&xLastWakeTime, 70);
    }
}
/**
 * @brief 温湿度数据获取任务
 */
void RunAHT20Task()
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint32_t count = 0;
    while (1)
    {
        if (++count >= 100)
        {
            debugger_printf("this update app version is 2.0!");
            count = 0;
            record_t record;
            uint8_t status = create_record(&record); // 构建record数据结构
            if (status == HAL_ERROR)
                printf("aht20 read failed\r\n");
            if (IPC_Report(record) == 0)
                printf("in Queue failed\r\n");
        }
        g_last_beat[TASK_AHT20_IDX] = xTaskGetTickCount(); /* 每100ms刷心跳 */

        vTaskDelayUntil(&xLastWakeTime, AHT20_TASK_PERIOD);
    }
}
/**
 * @brief 网络任务
 */
void RunESP8266Task()
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    record_t record_data;
    char *json_data = NULL;
    uint8_t cache_due = 0;
    while (1)
    {
        EspState status = ESP8266_Task();
        if (IPC_ReceiveAht20Data(&record_data)) // 判断是否有实时数据可发送
        {
            if (status == READY)
            {
                json_data = get_json_data(record_data);
                // 如果队列有数据就上报
                Publish_MQTT_Message(json_data);
                free(json_data);
                cache_due = 1;
            }
            else
            {
                if (record_data.timestamp > 0) // 脏数据不存
                    IPC_SendCache(record_data);
            }
        }
        if (status == READY && cache_due == 1 && get_publish_pending() == 0)
        {
            cache_due = 0;
            record_t *record = resend_data();
            char *json_data = NULL;
            if (record)
            {
                json_data = get_json_data(*record);
                Publish_MQTT_Message(json_data);
                free(json_data);
                flash_delete_record(); // 清magic魔数
                debugger_printf("record%p\n", record);
            }
        }
        g_last_beat[TASK_ESP8266_IDX] = xTaskGetTickCount();
        vTaskDelayUntil(&xLastWakeTime, ESP8266_TASK_PERIOD);
    }
}

/**
 * @brief 离线缓存任务
 */

void cacheTask()
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    record_t record_data;
    while (1)
    {
        if (IPC_ReceiveAht20DataFromCache(&record_data)) // 队列接收缓存数据
        {
            offline_cache_record(record_data); // 写入缓存
        }
        g_last_beat[TASK_CACHE_IDX] = xTaskGetTickCount();
        vTaskDelayUntil(&xLastWakeTime, CACHE_PERIOD);
    }
}

// /**
//  * @brief 重发任务
//  */
// void resendTask()
// {
//     TickType_t xLastWakeTime = xTaskGetTickCount();
//     uint32_t ulNotifiedValue = 0; // 清除通知值之前的快照
//     while (1)
//     {
//         if (xTaskNotifyWait(0x00, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY) == pdTRUE)
//         {
//         }
//         vTaskDelayUntil(&xLastWakeTime, reSend_PERIOD);
//     }
// }

void Debug_Task()
{
    // UBaseType_t watermark;
    // eTaskState task_state;
    // while (1)
    // {
    //     // ========== 查看AHT20任务 ==========
    //     watermark = uxTaskGetStackHighWaterMark(AHT20Task_Handle);
    //     task_state = eTaskGetState(AHT20Task_Handle);
    //     debugger_printf("AHT20: watermark=%d words, state=%d\r\n", watermark, task_state);

    //     // ========== 查看ESP8266任务 ==========
    //     watermark = uxTaskGetStackHighWaterMark(RunESP8266_Handle);
    //     task_state = eTaskGetState(RunESP8266_Handle);
    //     debugger_printf("ESP8266: watermark=%d words, state=%d\r\n", watermark, task_state);

    //     vTaskDelay(pdMS_TO_TICKS(Debug_PERIOD));
    // }
}