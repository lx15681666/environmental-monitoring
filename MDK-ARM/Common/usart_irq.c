#include "usart_irq.h"
#include "main.h"
// uint8_t rx_buffer[RX_BUFFER_SIZE];
// uint16_t rx_index = 0;
// uint8_t rx_complete_flag = 0; // 接收完成标志
volatile uint16_t rx_wifi_index = 0;
uint8_t rx_wifi_buffer[RX_BUFFER_SIZE];
volatile uint8_t rx_wifi_byte = 0;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{

    // if (huart->Instance == USART1) // 检查是否为USART1
    // {
    //     if (rx_buffer[rx_index] == '*')
    //     {
    //         rx_buffer[rx_index] = '\0'; // 添加字符串结束符
    //         rx_complete_flag = 1;       // 设置接收完成标志
    //         rx_index = 0;               // 重置索引等待下一次指令的接收
    //     }
    //     else
    //     {
    //         rx_index++;                     // 增加索引
    //         if (rx_index >= RX_BUFFER_SIZE) // 检查是否超出缓冲区大小
    //         {
    //             rx_index = 0; // 重置索引
    //         }
    //     }
    //     // 继续接收下一个字节
    //     HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_buffer[rx_index], 1);
    // }
    if (huart->Instance == USART2)
    {
        uint8_t received_byte = rx_wifi_byte;
        if (rx_wifi_index < RX_BUFFER_SIZE - 1U)
        {
            rx_wifi_buffer[rx_wifi_index++] = received_byte;
            rx_wifi_buffer[rx_wifi_index] = '\0';
        }
        else
        {
            rx_wifi_index = 0U;
            rx_wifi_buffer[0] = '\0';
        }
        HAL_UART_Receive_IT(&huart2, (uint8_t *)&rx_wifi_byte, 1);
    }
}
// 处理接收完成事件

void ESP8266_SendCommand(const char *cmd)
{
    __disable_irq();
    memset(rx_wifi_buffer, 0, sizeof(rx_wifi_buffer));
    rx_wifi_index = 0;
    __enable_irq();
    printf("Send: %s\r\n", cmd);
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);
}

void ESP8266_SendRaw(const char *data)
{
    __disable_irq();
    memset(rx_wifi_buffer, 0, sizeof(rx_wifi_buffer));
    rx_wifi_index = 0;
    __enable_irq();

    HAL_UART_Transmit(&huart2, (uint8_t *)data, strlen(data), HAL_MAX_DELAY);
}
