#include "App_freeRTOS.h"
#include "Int_AHT20.h"
#include "Int_ESP8266.h"

void RunESP8266Task(void *arg);
#define RunESP8266_STACK_SIZE 1024 // 一般最小是128个字，一个字等于4字节
#define RunESP8266_PRIORITY 2
TaskHandle_t RunESP8266_Handle;

void RunAHT20Task(void *arg);
#define AHT20Task_STACK_SIZE 128 // 一般最小是128个字，一个字等于4字节
#define AHT20Task_PRIORITY 2
TaskHandle_t AHT20Task_Handle;

void LEDTask(void *arg);
#define LEDTask_STACK_SIZE 128 // 一般最小是128个字，一个字等于4字节
#define LEDTask_PRIORITY 1
TaskHandle_t LEDTask_Handle;

void StartFreeRTOS()
{
    xTaskCreate(RunAHT20Task, "AHT20Task", AHT20Task_STACK_SIZE, NULL, AHT20Task_PRIORITY, &AHT20Task_Handle);
    xTaskCreate(RunESP8266Task, "RunESP8266Task", RunESP8266_STACK_SIZE, NULL, RunESP8266_PRIORITY, &RunESP8266_Handle);
    xTaskCreate(LEDTask, "LEDTask", LEDTask_STACK_SIZE, NULL, LEDTask_PRIORITY, &LEDTask_Handle);
    //启用任务调度
    vTaskStartScheduler();
}
void LEDTask(void *arg)
{
    while (1)
    {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_1); // Toggle the LED state
        // debugger_printf("LEDTask running\n");
        vTaskDelay(500); // 会释放cpu的占用
    }
}
void RunAHT20Task(void *arg)
{
    while (1)
    {
        AHT20Task(NULL);
        vTaskDelay(10000);
    }
}
void RunESP8266Task(void *arg)
{
    while (1)
    {
        ESP8266_Task();
        vTaskDelay(10);
    }
}